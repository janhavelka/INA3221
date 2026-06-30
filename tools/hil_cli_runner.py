#!/usr/bin/env python3
"""Bounded serial HIL runner for the INA3221 example CLI.

The script intentionally stays small: it drives the existing example commands,
classifies their transcripts, and can emit Markdown evidence for manual review.
It does not simulate devices or bypass the firmware command surface.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import os
import platform
import re
import statistics
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Optional


ANSI_RE = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
DEFAULT_FAILURE_TOKENS = (
    "[E]",
    "[FAIL]",
    "Guru Meditation",
    "assert failed",
    "panic",
    "Status: I2C_",
    "Status: TIMEOUT",
    "Status: DEVICE_NOT_FOUND",
    "Status: MANUFACTURER_ID_MISMATCH",
    "Status: DIE_ID_MISMATCH",
    "Driver is offline",
    "Failed to initialize",
)
PROMPT_RE = re.compile(r"(?:^|\n)>\s*$")


@dataclass(frozen=True)
class Step:
    test_id: str
    feature: str
    command: str
    expected: tuple[str, ...]
    notes: str = ""
    timeout_s: Optional[float] = None
    failure_tokens: tuple[str, ...] = DEFAULT_FAILURE_TOKENS


@dataclass
class StepResult:
    test_id: str
    feature: str
    command: str
    expected: str
    observed: str
    elapsed_s: float
    status: str
    notes: str = ""


@dataclass
class SoakSummary:
    status: str = "NOT RUN"
    start_iso: str = ""
    end_iso: str = ""
    elapsed_s: float = 0.0
    command_counts: dict[str, int] = field(default_factory=dict)
    pass_count: int = 0
    fail_count: int = 0
    unknown_count: int = 0
    consecutive_failure_bursts: int = 0
    max_consecutive_failures: int = 0
    min_latency_s: float = 0.0
    mean_latency_s: float = 0.0
    max_latency_s: float = 0.0
    worst_read_latency_s: float = 0.0
    reset_recovery_count: int = 0
    stop_reason: str = ""
    failure_samples: list[StepResult] = field(default_factory=list)


def strip_ansi(text: str) -> str:
    return ANSI_RE.sub("", text)


def normalize(text: str) -> str:
    return strip_ansi(text).replace("\r\n", "\n").replace("\r", "\n")


def compact(text: str, limit: int = 220) -> str:
    cleaned = " ".join(normalize(text).split())
    if len(cleaned) <= limit:
        return cleaned
    return cleaned[: limit - 3] + "..."


def markdown_escape(text: str) -> str:
    return text.replace("\\", "\\\\").replace("|", "\\|").replace("\n", "<br>")


def classify_output(
    output: str,
    expected: Iterable[str],
    failure_tokens: Iterable[str],
    timed_out: bool,
) -> tuple[str, str]:
    text = normalize(output)
    if timed_out:
        return "FAIL", "command timed out before CLI prompt"

    missing = [token for token in expected if token not in text]
    failures = [token for token in failure_tokens if token and token in text]

    if failures:
        return "FAIL", "unexpected failure token(s): " + ", ".join(failures)
    if missing:
        return "FAIL", "missing expected token(s): " + ", ".join(missing)
    return "PASS", "expected token(s) observed"


def run_parser_self_test() -> int:
    cases = [
        (
            "ansi pass",
            "\x1b[36m[I]\x1b[0m === Version Info ===\nINA3221 library version: 2.0.0\n> ",
            ("Version Info", "INA3221 library version"),
            DEFAULT_FAILURE_TOKENS,
            False,
            "PASS",
        ),
        (
            "failure token",
            "[E] Failed to initialize\n> ",
            ("Status: OK",),
            DEFAULT_FAILURE_TOKENS,
            False,
            "FAIL",
        ),
        (
            "expected invalid input",
            "[W] Invalid avg (0-7)\n> ",
            ("Invalid avg",),
            (),
            False,
            "PASS",
        ),
        (
            "missing expected",
            "hello\n> ",
            ("Version Info",),
            DEFAULT_FAILURE_TOKENS,
            False,
            "FAIL",
        ),
        (
            "timeout",
            "partial",
            ("Version Info",),
            DEFAULT_FAILURE_TOKENS,
            True,
            "FAIL",
        ),
    ]

    failures = 0
    for name, output, expected, failure_tokens, timed_out, want in cases:
        got, note = classify_output(output, expected, failure_tokens, timed_out)
        ok = got == want
        print(f"{name}: {'PASS' if ok else 'FAIL'} ({got}: {note})")
        if not ok:
            failures += 1
    return 1 if failures else 0


class SerialSession:
    def __init__(self, port: str, baud: int, read_timeout_s: float) -> None:
        try:
            import serial  # type: ignore
        except ImportError as exc:  # pragma: no cover - depends on host setup
            raise RuntimeError(
                "pyserial is required for hardware runs; install the 'serial' package"
            ) from exc

        self._serial_mod = serial
        self.ser = serial.Serial(
            port=port,
            baudrate=baud,
            timeout=read_timeout_s,
            write_timeout=read_timeout_s,
        )
        # Match platformio.ini defaults for USB CDC targets.
        self.ser.dtr = False
        self.ser.rts = False

    def close(self) -> None:
        self.ser.close()

    def reset_target(self, settle_s: float) -> None:
        # Keep BOOT/GPIO0 released and pulse reset only. Some ESP32-S3 USB
        # bridges enter the ROM downloader if DTR and RTS are asserted together.
        self.ser.dtr = False
        self.ser.rts = True
        time.sleep(0.1)
        self.ser.rts = False
        time.sleep(settle_s)

    def read_available_for(self, duration_s: float) -> str:
        deadline = time.monotonic() + duration_s
        buf = bytearray()
        while time.monotonic() < deadline:
            waiting = getattr(self.ser, "in_waiting", 0)
            chunk = self.ser.read(waiting or 1)
            if chunk:
                buf.extend(chunk)
        return buf.decode(errors="replace")

    def send_command(self, command: str) -> None:
        self.ser.write((command + "\r\n").encode("utf-8"))
        self.ser.flush()

    def read_until_prompt(self, timeout_s: float, idle_timeout_s: float) -> tuple[str, bool]:
        _ = idle_timeout_s
        deadline = time.monotonic() + timeout_s
        buf = bytearray()
        prompt_seen = False

        while time.monotonic() < deadline:
            waiting = getattr(self.ser, "in_waiting", 0)
            chunk = self.ser.read(waiting or 1)
            if chunk:
                buf.extend(chunk)
                if PROMPT_RE.search(normalize(buf.decode(errors="replace"))):
                    prompt_seen = True
                    break

        if prompt_seen:
            settle_deadline = time.monotonic() + 0.02
            while time.monotonic() < settle_deadline:
                waiting = getattr(self.ser, "in_waiting", 0)
                if waiting:
                    chunk = self.ser.read(waiting)
                    if chunk:
                        buf.extend(chunk)
                        settle_deadline = time.monotonic() + 0.02
                        continue
                time.sleep(0.002)

        return buf.decode(errors="replace"), prompt_seen


def default_steps(stress_count: int, stress_mix_count: int) -> list[Step]:
    long_timeout = max(20.0, float(max(stress_count, stress_mix_count)) * 0.25)
    return [
        Step("CONN-001", "Serial CLI", "version",
             ("Version Info", "INA3221 library version")),
        Step("CONN-002", "I2C discovery", "scan",
             ("Scan complete", "INA3221 recognized:"), timeout_s=12.0),
        Step("CONN-003", "Identity", "probe", ("Status: OK",)),
        Step("CONN-004", "Identity", "ids",
             ("Manufacturer ID: 0x5449", "Die ID: 0x3220")),
        Step("STATE-001", "Lifecycle/health", "drv",
             ("Driver Health", "State:", "Total success")),
        Step("STATE-002", "Settings/cache", "settings",
             ("Cached Settings", "Hardware config dirty")),
        Step("DATA-001", "Timing", "timing", ("Timing Info", "Cycle time")),
        Step("DATA-002", "Config", "config", ("Config:", "Mode:")),
        Step("DATA-003", "Aggregate read", "read", ("CH1:",)),
        Step("DATA-004", "Channel read", "ch 1", ("CH1:",)),
        Step("DATA-005", "Raw shunt read", "shuntraw 1", ("CH1 shunt raw",)),
        Step("DATA-006", "Raw bus read", "busraw 1", ("CH1 bus raw",)),
        Step("DATA-007", "Shunt float read", "shunt 1", ("CH1 shunt",)),
        Step("DATA-008", "Bus float read", "bus 1", ("CH1 bus",)),
        Step("DATA-009", "Current read", "current 1", ("CH1 current",)),
        Step("DATA-010", "Power read", "power 1", ("CH1 power",)),
        Step("DATA-011", "Shunt sum raw", "sumraw", ("Shunt sum raw",)),
        Step("DATA-012", "Shunt sum float", "sum", ("Shunt sum",)),
        Step("MODE-001", "Mode show", "mode", ("Mode:",)),
        Step("MODE-002", "Power-down mode", "mode pd", ("Status: OK",)),
        Step("MODE-003", "Continuous restore", "mode sbc", ("Status: OK",)),
        Step("MODE-004", "Triggered mode", "mode sbtrig", ("Status: IN_PROGRESS",),
             failure_tokens=("[E]", "[FAIL]", "Guru Meditation", "panic")),
        Step("MODE-005", "Triggered mode poll", "poll", ("Conversion ready",)),
        Step("MODE-006", "Continuous restore", "mode sbc", ("Status: OK",)),
        Step("MODE-007", "Explicit triggered start", "start sbtrig", ("Status: IN_PROGRESS",),
             failure_tokens=("[E]", "[FAIL]", "Guru Meditation", "panic")),
        Step("MODE-008", "Triggered start poll", "poll", ("Conversion ready",)),
        Step("MODE-009", "Continuous restore", "mode sbc", ("Status: OK",)),
        Step("CFG-001", "Averaging lower bound", "avg 0", ("Status: OK",)),
        Step("CFG-002", "Averaging upper bound", "avg 7", ("Status: OK",)),
        Step("CFG-003", "Averaging restore", "avg 0", ("Status: OK",)),
        Step("CFG-004", "Bus CT lower bound", "vbusct 0", ("Status: OK",)),
        Step("CFG-005", "Bus CT upper bound", "vbusct 7", ("Status: OK",)),
        Step("CFG-006", "Bus CT restore", "vbusct 4", ("Status: OK",)),
        Step("CFG-007", "Shunt CT lower bound", "vshct 0", ("Status: OK",)),
        Step("CFG-008", "Shunt CT upper bound", "vshct 7", ("Status: OK",)),
        Step("CFG-009", "Shunt CT restore", "vshct 4", ("Status: OK",)),
        Step("CFG-010", "Disable channel", "chen 3 0", ("Status: OK",)),
        Step("CFG-011", "Disabled-channel read rejection", "ch 3",
             ("Status: INVALID_CONFIG", "Channel disabled"),
             failure_tokens=("[E]", "[FAIL]", "Guru Meditation", "panic")),
        Step("CFG-012", "Restore channel", "chen 3 1", ("Status: OK",)),
        Step("ALERT-001", "Alert flags", "alerts", ("Alert Flags",)),
        Step("ALERT-002", "Mask/Enable decode", "mask",
             ("Mask/Enable Register", "clears latched alert")),
        Step("ALERT-003", "Critical limits", "crit", ("critical limit",)),
        Step("ALERT-004", "Warning limits", "warn", ("warning limit",)),
        Step("ALERT-005", "Summation limit", "sumlim", ("Shunt sum limit",)),
        Step("ALERT-006", "Power-valid high", "pvhi", ("Power valid upper limit",)),
        Step("ALERT-007", "Power-valid low", "pvlo", ("Power valid lower limit",)),
        Step("ALERT-008", "Summation channels", "sumch", ("Mask/Enable Register",)),
        Step("ALERT-009", "Alert latch", "latch", ("Mask/Enable Register",)),
        Step("RESET-001", "Manual recovery", "recover", ("Status: OK",)),
        Step("RESET-002", "Software reset", "reset", ("Status: OK",)),
        Step("RESET-003", "Post-reset recovery", "recover", ("Status: OK",)),
        Step("RESET-004", "Post-reset settings", "settings",
             ("Cached Settings", "Hardware config dirty: NO")),
        Step("MATH-001", "Shunt conversion", "convert shunt -1", ("Shunt raw -1 =",)),
        Step("MATH-002", "Bus conversion high", "convert bus 32767", ("Bus raw 32767 =",)),
        Step("MATH-003", "Bus conversion negative", "convert bus -1", ("Bus raw -1 =",)),
        Step("ERR-001", "Invalid command", "unknown_hil_command",
             ("Unknown command",), failure_tokens=("[E]", "[FAIL]", "Guru Meditation", "panic")),
        Step("ERR-002", "Invalid channel", "ch 4",
             ("Invalid channel",), failure_tokens=("[E]", "[FAIL]", "Guru Meditation", "panic")),
        Step("ERR-003", "Invalid averaging", "avg 8",
             ("Invalid avg",), failure_tokens=("[E]", "[FAIL]", "Guru Meditation", "panic")),
        Step("ERR-004", "Invalid conversion time", "vbusct x",
             ("Invalid conv time",), failure_tokens=("[E]", "[FAIL]", "Guru Meditation", "panic")),
        Step("ERR-005", "Invalid mode", "mode nope",
             ("Invalid mode",), failure_tokens=("[E]", "[FAIL]", "Guru Meditation", "panic")),
        Step("ERR-006", "Invalid register", "reg 0x100",
             ("Usage: reg",), failure_tokens=("[E]", "[FAIL]", "Guru Meditation", "panic")),
        Step("ERR-007", "Invalid stress count", "stress 0",
             ("Invalid count",), failure_tokens=("[E]", "[FAIL]", "Guru Meditation", "panic")),
        Step("STRESS-001", "Self-test", "selftest",
             ("INA3221 selftest", "Selftest result:", "fail=0"), timeout_s=15.0),
        Step("STRESS-002", "Measurement stress", f"stress {stress_count}",
             ("Stress Summary", "Errors: 0"), timeout_s=long_timeout),
        Step("STRESS-003", "Mixed stress", f"stress_mix {stress_mix_count}",
             ("stress_mix summary", "fail=0"), timeout_s=long_timeout),
        Step("FINAL-001", "Final health", "drv",
             ("Driver Health", "State:", "Total failures")),
    ]


def benchmark_steps(count: int) -> list[Step]:
    timeout_s = max(20.0, float(count) * 0.25)
    return [
        Step("BENCH-001", "Aggregate sample benchmark", f"read {count}",
             ("Reading", "CH1:"), timeout_s=timeout_s),
        Step("BENCH-002", "Measurement stress benchmark", f"stress {count}",
             ("Stress Summary", "Errors: 0"), timeout_s=timeout_s),
        Step("BENCH-003", "Mixed-operation benchmark", f"stress_mix {count}",
             ("stress_mix summary", "fail=0"), timeout_s=timeout_s),
    ]


def run_step(
    session: SerialSession,
    step: Step,
    default_timeout_s: float,
    idle_timeout_s: float,
    resync_timeout_s: float,
    transcript: list[str],
    verbose: bool,
    command_pacing_s: float,
) -> StepResult:
    timeout_s = step.timeout_s if step.timeout_s is not None else default_timeout_s
    transcript.append(f"\n===== {step.test_id} :: {step.command} =====\n")
    if verbose:
        print(f"{step.test_id}: {step.command}")

    start = time.monotonic()
    session.send_command(step.command)
    output, prompt_seen = session.read_until_prompt(timeout_s, idle_timeout_s)
    elapsed = time.monotonic() - start
    transcript.append(output)

    status, note = classify_output(
        output,
        expected=step.expected,
        failure_tokens=step.failure_tokens,
        timed_out=not prompt_seen,
    )
    if step.notes:
        note = f"{note}; {step.notes}"
    if not prompt_seen and resync_timeout_s > 0.0:
        transcript.append(f"\n===== RESYNC after {step.test_id} =====\n")
        extra, resynced = session.read_until_prompt(resync_timeout_s, idle_timeout_s)
        transcript.append(extra)
        note = f"{note}; resync {'ok' if resynced else 'failed'}"
    if verbose:
        print(f"  {status} {elapsed:.3f}s {note}")

    result = StepResult(
        test_id=step.test_id,
        feature=step.feature,
        command=step.command,
        expected=", ".join(step.expected),
        observed=compact(output),
        elapsed_s=elapsed,
        status=status,
        notes=note,
    )
    if command_pacing_s > 0.0:
        time.sleep(command_pacing_s)
    return result


def run_soak(
    session: SerialSession,
    duration_s: float,
    default_timeout_s: float,
    idle_timeout_s: float,
    resync_timeout_s: float,
    transcript: list[str],
    verbose: bool,
    failure_limit: int,
    command_pacing_s: float,
    pacing_s: float,
    capture_transcript: bool,
) -> SoakSummary:
    summary = SoakSummary(status="PASS")
    summary.start_iso = now_iso()
    deadline = time.monotonic() + duration_s
    latencies: list[float] = []
    read_latencies: list[float] = []
    consecutive_failures = 0

    cycle = [
        Step("SOAK-RD", "Soak read", "read", ("CH1:",)),
        Step("SOAK-RAW-S", "Soak raw shunt", "shuntraw 1", ("CH1 shunt raw",)),
        Step("SOAK-RAW-B", "Soak raw bus", "busraw 1", ("CH1 bus raw",)),
        Step("SOAK-I", "Soak current", "current 1", ("CH1 current",)),
        Step("SOAK-P", "Soak power", "power 1", ("CH1 power",)),
        Step("SOAK-RD", "Soak read", "read", ("CH1:",)),
        Step("SOAK-RAW-S", "Soak raw shunt", "shuntraw 1", ("CH1 shunt raw",)),
        Step("SOAK-RAW-B", "Soak raw bus", "busraw 1", ("CH1 bus raw",)),
        Step("SOAK-I", "Soak current", "current 1", ("CH1 current",)),
        Step("SOAK-P", "Soak power", "power 1", ("CH1 power",)),
        Step("SOAK-POLL", "Soak readiness", "poll", ("Conversion ready",)),
        Step("SOAK-MIX", "Soak mixed stress", "stress_mix 5",
             ("stress_mix summary", "fail=0"), timeout_s=max(10.0, default_timeout_s)),
        Step("SOAK-SET", "Soak settings", "settings", ("Cached Settings",)),
        Step("SOAK-DRV", "Soak health", "drv", ("Driver Health",)),
        Step("SOAK-PROBE", "Soak probe", "probe", ("Status: OK",)),
        Step("SOAK-REC", "Soak recover", "recover", ("Status: OK",)),
    ]

    idx = 0
    while time.monotonic() < deadline:
        step = cycle[idx % len(cycle)]
        idx += 1
        result = run_step(
            session,
            step,
            default_timeout_s,
            idle_timeout_s,
            resync_timeout_s,
            transcript if capture_transcript else [],
            verbose,
            command_pacing_s,
        )
        summary.command_counts[step.command] = summary.command_counts.get(step.command, 0) + 1
        latencies.append(result.elapsed_s)
        if "read" in step.command or "raw" in step.command or step.command in ("current 1", "power 1"):
            read_latencies.append(result.elapsed_s)
        if step.command == "recover":
            summary.reset_recovery_count += 1

        if result.status == "PASS":
            summary.pass_count += 1
            consecutive_failures = 0
        elif result.status == "UNKNOWN":
            summary.unknown_count += 1
        else:
            summary.fail_count += 1
            if len(summary.failure_samples) < 10:
                summary.failure_samples.append(result)
            consecutive_failures += 1
            summary.max_consecutive_failures = max(
                summary.max_consecutive_failures, consecutive_failures
            )
            if consecutive_failures == 1:
                summary.consecutive_failure_bursts += 1
            if "resync failed" in result.notes:
                summary.status = "FAIL"
                summary.stop_reason = (
                    f"stopped after prompt timeout and failed resync at {result.test_id}"
                )
                break
            if consecutive_failures >= failure_limit:
                summary.status = "FAIL"
                summary.stop_reason = (
                    f"stopped after {consecutive_failures} consecutive failed commands"
                )
                break
        if pacing_s > 0.0:
            time.sleep(pacing_s)

    summary.end_iso = now_iso()
    summary.elapsed_s = max(0.0, duration_s - max(0.0, deadline - time.monotonic()))
    if latencies:
        summary.min_latency_s = min(latencies)
        summary.mean_latency_s = statistics.fmean(latencies)
        summary.max_latency_s = max(latencies)
    if read_latencies:
        summary.worst_read_latency_s = max(read_latencies)
    if not summary.stop_reason:
        summary.stop_reason = "completed requested duration"
    return summary


def now_iso() -> str:
    return _dt.datetime.now().astimezone().isoformat(timespec="seconds")


def command_output(command: list[str]) -> str:
    try:
        proc = subprocess.run(
            command,
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=10,
        )
        return proc.stdout.strip() or f"exit={proc.returncode}"
    except Exception as exc:  # pragma: no cover - depends on host tools
        return f"not available: {exc}"


def git_value(args: list[str]) -> str:
    return command_output(["git", *args])


def write_markdown_report(
    path: Path,
    transcript_path: Optional[Path],
    args: argparse.Namespace,
    results: list[StepResult],
    boot_transcript: str,
    soak: SoakSummary,
    started_iso: str,
    ended_iso: str,
    run_error: str = "",
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    counts: dict[str, int] = {"PASS": 0, "FAIL": 0, "UNKNOWN": 0, "NOT RUN": 0}
    for result in results:
        counts[result.status] = counts.get(result.status, 0) + 1

    lines: list[str] = []
    lines.append(f"# INA3221 HIL Validation - {args.port or 'NO_PORT'}")
    lines.append("")
    lines.append(f"- Started: `{started_iso}`")
    lines.append(f"- Ended: `{ended_iso}`")
    lines.append(f"- Repository: `{Path.cwd()}`")
    lines.append(f"- Branch: `{git_value(['branch', '--show-current'])}`")
    lines.append(f"- Commit: `{git_value(['rev-parse', 'HEAD'])}`")
    lines.append(f"- Dirty status: `{git_value(['status', '--short']) or 'clean'}`")
    lines.append(f"- Host: `{platform.platform()}`")
    lines.append(f"- Python: `{sys.version.split()[0]}`")
    lines.append(f"- PlatformIO: `{command_output(['pio', '--version'])}`")
    lines.append(f"- Serial: `{args.port or 'not set'}` at `{args.baud}` baud")
    lines.append(f"- Per-command timeout: `{args.timeout_s}` s")
    lines.append(f"- Idle timeout: `{args.idle_timeout_s}` s")
    lines.append(f"- Timeout resync: `{args.resync_timeout_s}` s")
    lines.append(f"- Boot settle: `{args.boot_settle_s}` s")
    lines.append(f"- Command pacing: `{args.command_pacing_ms}` ms")
    lines.append(f"- Soak pacing: `{args.soak_pacing_ms}` ms")
    lines.append(f"- Soak transcript capture: `{'disabled' if args.no_soak_transcript else 'enabled'}`")
    if run_error:
        lines.append(f"- Run error: `{run_error}`")
    lines.append("")
    lines.append("## Summary")
    lines.append("")
    lines.append("| PASS | FAIL | UNKNOWN | NOT RUN |")
    lines.append("|---:|---:|---:|---:|")
    lines.append(
        f"| {counts.get('PASS', 0)} | {counts.get('FAIL', 0)} | "
        f"{counts.get('UNKNOWN', 0)} | {counts.get('NOT RUN', 0)} |"
    )
    lines.append("")
    lines.append("## Boot Transcript")
    lines.append("")
    if transcript_path:
        lines.append(f"Raw transcript: `{transcript_path}`")
    lines.append("")
    lines.append("```text")
    lines.append(compact(boot_transcript, 1200))
    lines.append("```")
    lines.append("")
    lines.append("## Detailed Results")
    lines.append("")
    lines.append(
        "| Test ID | Feature | Command | Expected | Observed | Elapsed s | Result | Notes |"
    )
    lines.append("|---|---|---|---|---|---:|---|---|")
    for result in results:
        lines.append(
            "| "
            + " | ".join(
                [
                    markdown_escape(result.test_id),
                    markdown_escape(result.feature),
                    markdown_escape(result.command),
                    markdown_escape(result.expected),
                    markdown_escape(result.observed),
                    f"{result.elapsed_s:.3f}",
                    markdown_escape(result.status),
                    markdown_escape(result.notes),
                ]
            )
            + " |"
        )
    lines.append("")
    lines.append("## Soak Summary")
    lines.append("")
    lines.append(f"- Result: `{soak.status}`")
    lines.append(f"- Start: `{soak.start_iso or 'not run'}`")
    lines.append(f"- End: `{soak.end_iso or 'not run'}`")
    lines.append(f"- Duration: `{soak.elapsed_s:.1f}` s")
    lines.append(f"- Pass/fail/unknown: `{soak.pass_count}/{soak.fail_count}/{soak.unknown_count}`")
    lines.append(f"- Max consecutive failures: `{soak.max_consecutive_failures}`")
    lines.append(f"- Worst command latency: `{soak.max_latency_s:.3f}` s")
    lines.append(f"- Worst read latency: `{soak.worst_read_latency_s:.3f}` s")
    lines.append(f"- Recover commands: `{soak.reset_recovery_count}`")
    lines.append(f"- Stop reason: `{soak.stop_reason or 'not run'}`")
    if soak.command_counts:
        lines.append("")
        lines.append("| Command | Count |")
        lines.append("|---|---:|")
        for command, count in sorted(soak.command_counts.items()):
            lines.append(f"| `{markdown_escape(command)}` | {count} |")
    lines.append("")
    if soak.failure_samples:
        lines.append("### Soak Failure Samples")
        lines.append("")
        lines.append("| Test ID | Command | Observed | Elapsed s | Notes |")
        lines.append("|---|---|---|---:|---|")
        for failure in soak.failure_samples:
            lines.append(
                "| "
                + " | ".join(
                    [
                        markdown_escape(failure.test_id),
                        markdown_escape(failure.command),
                        markdown_escape(failure.observed),
                        f"{failure.elapsed_s:.3f}",
                        markdown_escape(failure.notes),
                    ]
                )
                + " |"
            )
        lines.append("")
    lines.append("## Limitations")
    lines.append("")
    lines.append("- Electrical fault injection, disconnect testing, and unsafe stimulus are not attempted by this runner.")
    lines.append("- Raw register writes are intentionally excluded from the default suite.")
    lines.append("- Fixture-specific wiring and load plausibility require manual confirmation.")
    if args.no_soak_transcript:
        lines.append("- Per-command soak transcript capture was disabled to keep long soak runs bounded in memory; functional pre-soak transcript and soak summary counters are still reported.")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def dry_run(args: argparse.Namespace) -> int:
    steps = default_steps(args.stress_count, args.stress_mix_count)
    if args.sample_benchmark:
        steps.extend(benchmark_steps(args.benchmark_count))
    print("Dry run: no serial port opened.")
    print(f"Port: {args.port or 'not set'} baud={args.baud}")
    for step in steps:
        print(f"{step.test_id:10s} {step.feature:28s} {step.command}")
    if args.soak_hours or args.soak_seconds:
        duration = args.soak_seconds or (args.soak_hours * 3600.0)
        print(f"SOAK       would run for {duration:.1f} seconds")
    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial port, for example COM5")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate")
    parser.add_argument("--timeout-s", type=float, default=5.0, help="Per-command timeout")
    parser.add_argument("--idle-timeout-s", type=float, default=0.3, help="Read idle timeout")
    parser.add_argument("--resync-timeout-s", type=float, default=5.0,
                        help="Extra prompt wait after a timed-out command before continuing")
    parser.add_argument("--boot-settle-s", type=float, default=2.0, help="Boot/reset settle time")
    parser.add_argument("--command-pacing-ms", type=float, default=250.0,
                        help="Delay after each command before sending the next one")
    parser.add_argument("--reset", action="store_true", help="Pulse RTS for an app reset before reading boot output")
    parser.add_argument("--continue-after-connect-fail", action="store_true",
                        help="Continue the suite even if the first command is not responsive")
    parser.add_argument("--verbose", action="store_true", help="Print per-step progress")
    parser.add_argument("--dry-run", action="store_true", help="List commands without opening serial")
    parser.add_argument("--parser-self-test", action="store_true", help="Test transcript classifier")
    parser.add_argument("--markdown-report", type=Path, help="Write Markdown report")
    parser.add_argument("--transcript-file", type=Path, help="Write raw transcript")
    parser.add_argument("--stress-count", type=int, default=50, help="Count for stress command")
    parser.add_argument("--stress-mix-count", type=int, default=50, help="Count for stress_mix command")
    parser.add_argument("--sample-benchmark", action="store_true", help="Append benchmark steps")
    parser.add_argument("--benchmark-count", type=int, default=50, help="Benchmark sample count")
    parser.add_argument("--soak-hours", type=float, default=0.0, help="Optional soak duration in hours")
    parser.add_argument("--soak-seconds", type=float, default=0.0, help="Optional soak duration in seconds")
    parser.add_argument("--soak-failure-limit", type=int, default=3,
                        help="Stop soak after this many consecutive failures")
    parser.add_argument("--soak-pacing-ms", type=float, default=250.0,
                        help="Optional delay between soak commands")
    parser.add_argument("--no-soak-transcript", action="store_true",
                        help="Do not retain per-command soak transcripts in memory")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.parser_self_test:
        return run_parser_self_test()
    if args.dry_run:
        return dry_run(args)
    if not args.port:
        print("--port is required unless --dry-run or --parser-self-test is used", file=sys.stderr)
        return 2

    started_iso = now_iso()
    ended_iso = started_iso
    transcript: list[str] = []
    boot_transcript = ""
    results: list[StepResult] = []
    soak = SoakSummary(status="NOT RUN", stop_reason="soak not requested")
    run_error = ""

    transcript_path = args.transcript_file
    if transcript_path is None and args.markdown_report:
        transcript_path = args.markdown_report.with_suffix(".transcript.txt")

    session: Optional[SerialSession] = None
    try:
        session = SerialSession(args.port, args.baud, read_timeout_s=0.05)
        if args.reset:
            session.reset_target(args.boot_settle_s)
        else:
            time.sleep(args.boot_settle_s)
        boot_transcript = session.read_available_for(args.idle_timeout_s)
        transcript.append("===== BOOT / PRE-COMMAND TRANSCRIPT =====\n")
        transcript.append(boot_transcript)

        steps = default_steps(args.stress_count, args.stress_mix_count)
        if args.sample_benchmark:
            steps.extend(benchmark_steps(args.benchmark_count))
        suite_aborted = False
        for index, step in enumerate(steps):
            result = run_step(
                session,
                step,
                args.timeout_s,
                args.idle_timeout_s,
                args.resync_timeout_s,
                transcript,
                args.verbose,
                max(0.0, args.command_pacing_ms / 1000.0),
            )
            results.append(result)
            if (
                index == 0
                and result.status != "PASS"
                and not args.continue_after_connect_fail
            ):
                reason = "CLI did not pass the initial responsiveness check"
                for skipped in steps[index + 1:]:
                    results.append(
                        StepResult(
                            test_id=skipped.test_id,
                            feature=skipped.feature,
                            command=skipped.command,
                            expected=", ".join(skipped.expected),
                            observed="not run",
                            elapsed_s=0.0,
                            status="NOT RUN",
                            notes=reason,
                        )
                    )
                soak = SoakSummary(status="NOT RUN", stop_reason=reason)
                suite_aborted = True
                break
            if "resync failed" in result.notes:
                reason = f"prompt timeout and failed resync at {step.test_id}"
                for skipped in steps[index + 1:]:
                    results.append(
                        StepResult(
                            test_id=skipped.test_id,
                            feature=skipped.feature,
                            command=skipped.command,
                            expected=", ".join(skipped.expected),
                            observed="not run",
                            elapsed_s=0.0,
                            status="NOT RUN",
                            notes=reason,
                        )
                    )
                soak = SoakSummary(status="NOT RUN", stop_reason=reason)
                suite_aborted = True
                break

        soak_duration = args.soak_seconds or (args.soak_hours * 3600.0)
        if soak_duration > 0.0 and not suite_aborted:
            soak = run_soak(
                session,
                soak_duration,
                args.timeout_s,
                args.idle_timeout_s,
                args.resync_timeout_s,
                transcript,
                args.verbose,
                args.soak_failure_limit,
                max(0.0, args.command_pacing_ms / 1000.0),
                max(0.0, args.soak_pacing_ms / 1000.0),
                not args.no_soak_transcript,
            )
    except Exception as exc:
        run_error = str(exc)
        print(f"HIL run error: {run_error}", file=sys.stderr)
        if not results:
            for step in default_steps(args.stress_count, args.stress_mix_count):
                results.append(
                    StepResult(
                        test_id=step.test_id,
                        feature=step.feature,
                        command=step.command,
                        expected=", ".join(step.expected),
                        observed="not run",
                        elapsed_s=0.0,
                        status="NOT RUN",
                        notes=run_error,
                    )
                )
        soak = SoakSummary(status="NOT RUN", stop_reason=run_error)
    finally:
        if session is not None:
            session.close()
        ended_iso = now_iso()

    if transcript_path:
        transcript_path.parent.mkdir(parents=True, exist_ok=True)
        transcript_path.write_text("".join(transcript), encoding="utf-8", errors="replace")
    if args.markdown_report:
        write_markdown_report(
            args.markdown_report,
            transcript_path,
            args,
            results,
            boot_transcript,
            soak,
            started_iso,
            ended_iso,
            run_error,
        )

    counts: dict[str, int] = {"PASS": 0, "FAIL": 0, "UNKNOWN": 0, "NOT RUN": 0}
    for result in results:
        counts[result.status] = counts.get(result.status, 0) + 1
    print(
        "HIL summary: "
        f"PASS={counts.get('PASS', 0)} "
        f"FAIL={counts.get('FAIL', 0)} "
        f"UNKNOWN={counts.get('UNKNOWN', 0)} "
        f"NOT_RUN={counts.get('NOT RUN', 0)} "
        f"SOAK={soak.status}"
    )
    return 1 if counts.get("FAIL", 0) or run_error or soak.status == "FAIL" else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
