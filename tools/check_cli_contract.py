#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

REQUIRED_COMMON = [
    "BoardConfig.h",
    "BuildConfig.h",
    "Log.h",
    "I2cTransport.h",
    "I2cScanner.h",
    "CliStyle.h",
]

MANDATORY_COMMANDS = [
    "help", "version", "scan", "scanina", "read", "job", "mode", "avg",
    "vbusct", "vshct", "chen", "rshunt", "direction", "profile", "addr",
    "init", "end", "freq", "config", "reg", "wreg", "alerts", "alertsnap",
    "crit", "warn", "sumlim", "pvhi", "pvlo", "sumch", "latch", "drv",
    "diag", "verify", "mismatch", "probe", "recover", "online", "verbose",
    "stress", "stress_mix", "stress_owner", "stress_freq", "hilrun", "hilmark",
    "xfer_reset", "xfer_stats", "xfer_assert", "selftest",
]

REQUIRED_JOB_VARIANTS = [
    "job init", "job apply", "job reconcile", "job sample", "job continuous",
    "job powerdown", "job cancel", "job auto", "job step", "job result",
    "job lastsample", "job alerts",
]


def fail(msg: str) -> None:
    print(f"CLI contract FAILED: {msg}")
    raise SystemExit(1)


def ensure_exists(path: pathlib.Path, label: str) -> None:
    if not path.exists():
        fail(f"missing {label}: {path.as_posix()}")


def ensure_missing(path: pathlib.Path, label: str) -> None:
    if path.exists():
        fail(f"forbidden {label} still present: {path.as_posix()}")


def main() -> int:
    common_dir = ROOT / "examples" / "common"
    bringup_main = ROOT / "examples" / "01_basic_bringup_cli" / "main.cpp"

    ensure_exists(common_dir, "common example directory")
    ensure_exists(bringup_main, "bringup CLI example")

    ensure_missing(ROOT / "examples" / "00_smoke_boot", "deprecated example 00_smoke_boot")
    ensure_missing(
        ROOT / "examples" / "03_feature_walkthrough",
        "deprecated example 03_feature_walkthrough",
    )
    ensure_missing(common_dir / "IdfArduinoCompat.h", "Arduino compatibility facade")

    for name in REQUIRED_COMMON:
        ensure_exists(common_dir / name, f"common helper {name}")

    text = bringup_main.read_text(encoding="utf-8", errors="replace")
    wire_transport = (common_dir / "I2cTransport.h").read_text(
        encoding="utf-8", errors="replace"
    )
    local_case_one = re.findall(
        r"case\s+1\s*:\s*return\s+INA3221::Status::Error\("
        r"INA3221::Err::INVALID_PARAM,",
        wire_transport,
    )
    if len(local_case_one) != 2:
        fail("Wire local data-too-long result must remain a validation error")
    if re.search(
        r"INA3221::Err::INVALID_CONFIG,\s*"
        r'"Requested deadline is tighter than fixed Wire timeout"',
        wire_transport,
    ) is None:
        fail("Wire tighter-timeout rejection must remain INVALID_CONFIG")
    poll_context_count = len(
        re.findall(r"\bINA3221::PollContext\b", text)
    )
    wire_safe_budget_count = len(
        re.findall(r"\bwireSafeTransferBudget\s*\(", text)
    )
    if wire_safe_budget_count != poll_context_count + 1:
        fail("Every Arduino PollContext must use the fixed-Wire safe budget helper")
    for stale_label in (" TC=%d", "TimingCtl"):
        if stale_label in text:
            fail(f"Arduino CLI uses stale timing-control label '{stale_label}'")
    for label in ("TCF=%d", "TC_FAULT=%d", "TimingControl=%d",
                  "TimingControlFault=%d",
                  "TC_FAULT is the inverted TCF level"):
        if label not in text:
            fail(f"Arduino CLI timing-control label '{label}' is missing")
    for token in (
        "profile.mode = INA3221::Mode::SHUNT_BUS_TRIG;",
        "startTriggeredSample(",
    ):
        if token not in text:
            fail(f"owner-safe triggered example token '{token}' missing")

    for cmd in MANDATORY_COMMANDS:
        if re.search(rf"\b{re.escape(cmd)}\b", text) is None:
            fail(f"mandatory command '{cmd}' missing in {bringup_main.as_posix()}")

    for command in REQUIRED_JOB_VARIANTS:
        if f'"{command}' not in text:
            fail(f"owner command variant '{command}' missing")

    for token in (
        "Last error detail:",
        "Last error msg:",
        "Managed Register Verification Evidence",
        "mismatchExpected",
        "HIL_BEGIN token=",
        "XFER_ASSERT PASS",
        "I2C frequency must be 10000-400000 Hz",
        "Healthy INA3221 devices",
    ):
        if token not in text and token not in (ROOT / "include" / "INA3221" / "INA3221.h").read_text(encoding="utf-8"):
            fail(f"expanded diagnostic/automation token '{token}' missing")

    if re.search(r"\bcfg\b", text) is None and re.search(r"\bsettings\b", text) is None:
        fail("either 'cfg' or 'settings' command must be present")

    print("CLI contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
