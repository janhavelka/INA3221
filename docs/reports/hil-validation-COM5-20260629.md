# INA3221 HIL Validation - COM5

- Started: `2026-06-29T17:58:33+02:00`
- Ended: `2026-06-29T17:58:40+02:00`
- Repository: `C:\Users\Honza\Documents\Projects\INA3221`
- Branch: `main`
- Commit: `73996e252c6bf9a12be3c8dc62649782b03cc39f`
- Dirty status at report generation: `?? docs/reports/`, `?? tools/hil_cli_runner.py`
- Dirty status after report/tool documentation edits: `M CHANGELOG.md`, `M README.md`, `?? docs/reports/`, `?? tools/hil_cli_runner.py`
- Host: `Windows-11-10.0.26200-SP0`
- Python: `3.12.10`
- PlatformIO: `PlatformIO Core, version 6.1.18`
- Serial: `COM5` at `115200` baud
- Per-command timeout: `5.0` s
- Idle timeout: `0.5` s
- Boot settle: `2.0` s

## Summary

| PASS | FAIL | UNKNOWN | NOT RUN |
|---:|---:|---:|---:|
| 0 | 1 | 0 | 66 |

## Setup Record

- Target device/library: INA3221 triple-channel shunt/bus voltage monitor library.
- Library type: framework-neutral C++ driver with injected non-owning I2C transport.
- Example used for HIL firmware: `examples/01_basic_bringup_cli` via PlatformIO `esp32s3dev`.
- Supported hardware environments in this repo: ESP32-S3 DevKitC-1 and ESP32-S2 Saola Arduino examples, native host tests, and ESP-IDF component/example sources.
- Public API areas covered by the planned HIL suite: lifecycle, `probe()`, `recover()`, settings snapshot, health state, scalar and raw reads, conversion timing, channel enable/disable, alert limits, Mask/Enable diagnostics, reset, parser validation, stress, and conversion helpers.
- Serial endpoint requested by prompt: `COM5`.
- Upload endpoint observed: `COM5`, ESP32-S3, MAC `24:58:7c:db:db:ac`.
- Baud rate: `115200`.
- Default example I2C wiring from `examples/common/BoardConfig.h`: SDA `8`, SCL `9`, I2C `400000 Hz`, INA3221 address `0x40`, I2C timeout `50 ms`.
- Electrical safety assumption: no controlled load, disconnect harness, alert GPIO fixture, or fault-injection fixture was verified. No overvoltage, overcurrent, bus-short, disconnect, or unsafe stimulus tests were attempted.
- Detected INA3221 identity/address: `NOT RUN`; the firmware CLI did not become responsive on `COM5`.

## Exact Commands

```powershell
python tools\hil_cli_runner.py --parser-self-test
python tools\hil_cli_runner.py --dry-run --port COM5 --baud 115200 --sample-benchmark --soak-seconds 5
pio run -e esp32s3dev
pio run -e esp32s3dev -t upload --upload-port COM5
python tools\hil_cli_runner.py --port COM5 --baud 115200 --timeout-s 5 --idle-timeout-s 0.5 --boot-settle-s 2 --verbose --sample-benchmark --benchmark-count 20 --stress-count 20 --stress-mix-count 20 --soak-hours 8 --markdown-report docs\reports\hil-validation-COM5-20260629.md
```

Results:

- Parser self-test: `PASS`.
- Dry-run: `PASS`; command list printed without serial access.
- ESP32-S3 build: `SUCCESS`; RAM `22608 / 327680`, flash `394994 / 1310720`.
- ESP32-S3 upload to `COM5`: `SUCCESS`; chip ID and MAC above observed.
- HIL command suite: `FAIL` at `CONN-001` because `version` timed out before a CLI prompt.
- 8-hour soak: `NOT RUN`; blocked by failed CLI responsiveness check.

## HIL Tooling Work

- Added `tools/hil_cli_runner.py`, a small pyserial-based runner for the existing CLI.
- Added configurable `--port`, `--baud`, `--timeout-s`, `--idle-timeout-s`, boot settle, verbose transcript capture, Markdown report output, dry-run, parser self-test, benchmark count, stress counts, and bounded soak duration options.
- Added pass/fail/unknown/not-run classification with expected tokens and failure-token matching.
- Added early stop after failed CLI responsiveness so an unavailable endpoint records evidence instead of timing out every test.
- Adjusted optional reset handling after the first hardware attempt: the initial DTR/RTS reset put the ESP32-S3 into ROM download mode, so the runner now keeps BOOT/GPIO0 released and pulses RTS only.

## Command Surface Summary

- Safe baseline commands planned: `version`, `scan`, `probe`, `ids`, `drv`, `settings`, `timing`, `config`, scalar reads, raw reads, `mode`, `avg`, `vbusct`, `vshct`, `chen`, `alerts`, `mask`, alert-limit reads, `recover`, `reset`, conversion helpers, invalid-input checks, `selftest`, `stress`, and `stress_mix`.
- Intentionally excluded from the default runner: `wreg` and `config write`, because they are diagnostic raw writes that may desynchronize hardware and cache.
- Destructive status-read commands are labeled by expected output or report notes: `poll`, `alerts`, and `mask` read Mask/Enable and can clear CVRF or latched alert evidence.

## Boot Transcript

Raw transcript: `docs\reports\hil-validation-COM5-20260629.transcript.txt`

```text
ESP-ROM:esp32s3-20210327 Build:Mar 27 2021 rst:0x15 (USB_UART_CHIP_RESET),boot:0x0 (DOWNLOAD(USB/UART0)) Saved PC:0x40041a7c waiting for download
```

## Detailed Results

| Test ID | Feature | Command | Expected | Observed | Elapsed s | Result | Notes |
|---|---|---|---|---|---:|---|---|
| CONN-001 | Serial CLI | version | Version Info, INA3221 library version |  | 5.000 | FAIL | command timed out before CLI prompt |
| CONN-002 | I2C discovery | scan | Scan complete, INA3221 recognized: | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| CONN-003 | Identity | probe | Status: OK | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| CONN-004 | Identity | ids | Manufacturer ID: 0x5449, Die ID: 0x3220 | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| STATE-001 | Lifecycle/health | drv | Driver Health, State:, Total success | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| STATE-002 | Settings/cache | settings | Cached Settings, Hardware config dirty | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| DATA-001 | Timing | timing | Timing Info, Cycle time | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| DATA-002 | Config | config | Config:, Mode: | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| DATA-003 | Aggregate read | read | CH1: | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| DATA-004 | Channel read | ch 1 | CH1: | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| DATA-005 | Raw shunt read | shuntraw 1 | CH1 shunt raw | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| DATA-006 | Raw bus read | busraw 1 | CH1 bus raw | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| DATA-007 | Shunt float read | shunt 1 | CH1 shunt | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| DATA-008 | Bus float read | bus 1 | CH1 bus | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| DATA-009 | Current read | current 1 | CH1 current | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| DATA-010 | Power read | power 1 | CH1 power | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| DATA-011 | Shunt sum raw | sumraw | Shunt sum raw | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| DATA-012 | Shunt sum float | sum | Shunt sum | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| MODE-001 | Mode show | mode | Mode: | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| MODE-002 | Power-down mode | mode pd | Status: OK | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| MODE-003 | Continuous restore | mode sbc | Status: OK | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| MODE-004 | Triggered mode | mode sbtrig | Status: OK | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| MODE-005 | Triggered start | start | Status: IN_PROGRESS | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| MODE-006 | Triggered poll | poll | Conversion ready | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| MODE-007 | Continuous restore | mode sbc | Status: OK | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| CFG-001 | Averaging lower bound | avg 0 | Status: OK | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| CFG-002 | Averaging upper bound | avg 7 | Status: OK | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| CFG-003 | Averaging restore | avg 0 | Status: OK | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| CFG-004 | Bus CT lower bound | vbusct 0 | Status: OK | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| CFG-005 | Bus CT upper bound | vbusct 7 | Status: OK | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| CFG-006 | Bus CT restore | vbusct 4 | Status: OK | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| CFG-007 | Shunt CT lower bound | vshct 0 | Status: OK | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| CFG-008 | Shunt CT upper bound | vshct 7 | Status: OK | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| CFG-009 | Shunt CT restore | vshct 4 | Status: OK | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| CFG-010 | Disable channel | chen 3 0 | Status: OK | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| CFG-011 | Disabled-channel read rejection | ch 3 | Status: INVALID_CONFIG, Channel disabled | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| CFG-012 | Restore channel | chen 3 1 | Status: OK | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| ALERT-001 | Alert flags | alerts | Alert Flags | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| ALERT-002 | Mask/Enable decode | mask | Mask/Enable Register, clears latched alert | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| ALERT-003 | Critical limits | crit | critical limit | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| ALERT-004 | Warning limits | warn | warning limit | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| ALERT-005 | Summation limit | sumlim | Shunt sum limit | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| ALERT-006 | Power-valid high | pvhi | Power valid upper limit | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| ALERT-007 | Power-valid low | pvlo | Power valid lower limit | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| ALERT-008 | Summation channels | sumch | Mask/Enable Register | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| ALERT-009 | Alert latch | latch | Mask/Enable Register | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| RESET-001 | Manual recovery | recover | Status: OK | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| RESET-002 | Software reset | reset | Status: OK | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| RESET-003 | Post-reset recovery | recover | Status: OK | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| RESET-004 | Post-reset settings | settings | Cached Settings, Hardware config dirty: NO | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| MATH-001 | Shunt conversion | convert shunt -1 | Shunt raw -1 = | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| MATH-002 | Bus conversion high | convert bus 32767 | Bus raw 32767 = | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| MATH-003 | Bus conversion negative | convert bus -1 | Bus raw -1 = | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| ERR-001 | Invalid command | unknown_hil_command | Unknown command | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| ERR-002 | Invalid channel | ch 4 | Invalid channel | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| ERR-003 | Invalid averaging | avg 8 | Invalid avg | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| ERR-004 | Invalid conversion time | vbusct x | Invalid conv time | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| ERR-005 | Invalid mode | mode nope | Invalid mode | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| ERR-006 | Invalid register | reg 0x100 | Usage: reg | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| ERR-007 | Invalid stress count | stress 0 | Invalid count | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| STRESS-001 | Self-test | selftest | INA3221 selftest, Selftest result:, fail=0 | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| STRESS-002 | Measurement stress | stress 20 | Stress Summary, Errors: 0 | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| STRESS-003 | Mixed stress | stress_mix 20 | stress_mix summary, fail=0 | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| FINAL-001 | Final health | drv | Driver Health, State:, Total failures | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| BENCH-001 | Aggregate sample benchmark | read 20 | Reading, CH1: | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| BENCH-002 | Measurement stress benchmark | stress 20 | Stress Summary, Errors: 0 | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |
| BENCH-003 | Mixed-operation benchmark | stress_mix 20 | stress_mix summary, fail=0 | not run | 0.000 | NOT RUN | CLI did not pass the initial responsiveness check |

## 8-Hour Soak

- Result: `NOT RUN`
- Start: `not run`
- End: `not run`
- Duration: `0.0` s
- Pass/fail/unknown: `0/0/0`
- Max consecutive failures: `0`
- Worst command latency: `0.000` s
- Worst read latency: `0.000` s
- Recover commands: `0`
- Stop reason: `CLI did not pass the initial responsiveness check`

## Limitations

- Electrical fault injection, disconnect testing, and unsafe stimulus are not attempted by this runner.
- Raw register writes are intentionally excluded from the default suite.
- Fixture-specific wiring and load plausibility require manual confirmation.
- The required functional HIL tests, sampling benchmarks, and 8-hour soak did not run because the CLI did not respond on `COM5`.
- Other enumerated ESP serial ports were probed only with a harmless `version` command during diagnosis and belonged to other active projects, not this INA3221 target.

## Failures And Anomalies

1. `COM5` accepted upload and identified as ESP32-S3, but the app CLI did not respond to `version` within 5 seconds.
2. The boot transcript captured on `COM5` showed the ESP32-S3 ROM downloader: `boot:0x0 (DOWNLOAD(USB/UART0))`.
3. A first runner attempt using the previous reset sequence timed out at the outer command timeout; the process was stopped and the reset path was changed to avoid asserting BOOT and RESET together.
4. `pio device monitor -p COM5 -b 115200 --raw` produced no CLI output before the outer timeout and left a monitor process holding the port; that process was stopped before the final report run.

## Sampling And Timing Results

- Functional sampling rate: `NOT RUN`; no responsive CLI endpoint.
- Raw read timing: `NOT RUN`; no responsive CLI endpoint.
- Stress timing: `NOT RUN`; no responsive CLI endpoint.
- Worst observed HIL command latency: `5.000 s` for the failed `version` responsiveness check.

## Library Audit Findings

| Severity | Area | Evidence | Risk | Fix |
|---|---|---|---|---|
| High | HIL endpoint | `COM5` upload succeeds but CLI is not responsive; boot transcript shows ROM downloader | Hardware validation cannot exercise the library; future reports could falsely imply tested firmware | Implemented runner early-stop/reporting and safer reset pulse. Remaining fix: confirm the correct app serial endpoint or adjust documented/build USB serial configuration for the fixture. |
| Medium | `tick()` status visibility | Code inspection: `tick()` calls readiness polling and discards returned `Status` after the delay gate | Owner can only infer a failed readiness poll through health counters; destructive Mask/Enable reads remain easy to miss | Proposed: add or document a `Status`-returning poll path for owner-driven readiness. Native test: forced timeout/NACK during `tick()` after delay gate. HIL regression: `start`, bounded wait, `poll`, `drv` health delta. |
| Medium | `end()` teardown | Code inspection: `end()` performs best-effort power-down and clears state without observable shutdown status | Hardware may remain active while the driver reports `UNINIT` | Proposed: document `end()` as non-verifying teardown or add explicit `powerDown()` returning `Status`. Native test: failed power-down write behavior. |
| Medium | `readBlocking()` convenience behavior | Code inspection: bounded helper can loop/poll and can be called with no output pointers | Unsuitable for deadline-owned steady paths; all-null output is not useful | Proposed: keep convenience-only documentation and consider rejecting all-null outputs plus oversized timeout values. Native tests: all-null and timeout boundary cases. |
| Medium | Raw Mask/Enable reads | Code/docs inspection: `reg 0x06` and `readRegister16(REG_MASK_ENABLE)` have read-clear behavior | Diagnostics may clear CVRF or latched alert flags without obvious warning at raw API level | Proposed: add Doxygen/README note beside raw register helpers. Native test: fake Mask/Enable read-clear evidence. |
| Low | Raw diagnostic writes | Code inspection: `writeRegister16()` accepts valid writable and read-only addresses | Diagnostic writes to read-only registers can hide hardware-ignore/readback mismatch | Proposed: either reject known read-only registers for writes or document readback-required diagnostic policy. |
| Medium | `recover()` split failure | Code inspection: dirty flag exists for config/mask uncertainty, but split failure coverage can be narrower | A partial recover may leave hardware/cache uncertainty that needs clear regression coverage | Proposed: add fake-bus call-count test for config success followed by Mask/Enable failure, and HIL regression once CLI is responsive. |

## Fixes Implemented During This Pass

- Added `tools/hil_cli_runner.py`.
- Added report/transcript generation under `docs/reports/`.
- Updated README HIL documentation to reference the runner and preserve fixture limitations.
- Updated CHANGELOG unreleased notes for the runner.
- No core driver behavior was changed in this pass because functional HIL never reached the library command surface.

## Final Verification

| Command | Result |
|---|---|
| `python tools\hil_cli_runner.py --parser-self-test` | `PASS` |
| `python tools\hil_cli_runner.py --dry-run --port COM5 --baud 115200 --sample-benchmark --soak-seconds 5` | `PASS` |
| `python -m py_compile tools\hil_cli_runner.py` | `PASS` |
| `python tools\check_core_timing_guard.py` | `PASS` |
| `python scripts\generate_version.py check` | `PASS` |
| `python tools\check_cli_contract.py` | `PASS` |
| `python tools\check_idf_example_contract.py` | `PASS` |
| `pio test -e native` | `PASS`, 74/74 tests |
| `pio run -e esp32s3dev` | `SUCCESS` |
| `pio run -e esp32s3dev -t upload --upload-port COM5` | `SUCCESS`; ESP32-S3 MAC `24:58:7c:db:db:ac` |
| `python tools\hil_cli_runner.py --port COM5 ... --soak-hours 8 ...` | `FAIL` at CLI responsiveness; 0 pass, 1 fail, 66 not run |
| `pio pkg pack` | `PASS`; generated `INA3221-1.2.0.tar.gz` then removed |
| `git diff --check` | `PASS` with LF-to-CRLF warnings for `CHANGELOG.md` and `README.md` |

CI status was not checked from a remote provider in this pass.
