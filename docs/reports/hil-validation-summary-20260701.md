# INA3221 HIL Validation Summary - 2026-07-01

This file preserves the release-relevant HIL evidence after removing generated
Markdown reports, PID files, stdout/stderr logs, and raw transcripts. Full
transcripts are intentionally not retained in the repository.

## Environment

- Host: Windows 11 (`10.0.26200`)
- Python: `3.12.10`
- PlatformIO: `6.1.18`
- Library version under test: `2.0.0`
- `idf.py`: not available on `PATH`; pure ESP-IDF local build success is not claimed.
- Release checks use PlatformIO Arduino builds, native tests, parser/contract checks,
  and package validation unless CI or a local ESP-IDF environment supplies `idf.py`
  evidence.

## Key Finding

- ESP32-S3 USB Serial/JTAG HWCDC (`ARDUINO_USB_MODE=1`) can leave queued CLI
  output undrained during long HIL soak runs. The firmware kept servicing INA3221
  commands, but the host runner stopped receiving prompts and failed resync.
- ESP32-S3 TinyUSB CDC (`ARDUINO_USB_MODE=0`) with DTR asserted fixed the HIL
  serial path on the same board and workload.
- The release PlatformIO ESP32-S3 Arduino environment now uses TinyUSB CDC and
  the HIL runner exposes explicit `--dtr` / `--rts` line-state options.

## Final Release HIL Run

- Date: `2026-07-01`
- Port/backend: `COM30`, TinyUSB CDC, `DTR=1`, `RTS=0`
- Commit at run start: `20ccb26953ecf7fac1f47a9128850ad425290dea`
- Dirty files during run: `platformio.ini`, `tools/hil_cli_runner.py`, generated
  HIL artifacts
- Main suite: `PASS=69`, `FAIL=0`, `UNKNOWN=0`, `NOT_RUN=0`
- Soak result: `PASS`
- Soak duration: `7200.0` s
- Soak command outcomes: `13542/0/0` pass/fail/unknown
- Max consecutive failures: `0`
- Worst command latency: `0.047` s
- Worst read latency: `0.047` s
- Recovery commands during soak: `846`
- Stop reason: completed requested duration
- Final health evidence: `READY`, consecutive failures `0`, total failures `0`

The run covered the CLI command surface, identity reads, settings/cache display,
aggregate and channel reads, raw outputs, mode transitions, triggered polling,
channel disable rejection, alert/mask diagnostics, recovery/reset paths, math
conversion commands, invalid input handling, safe self-test, measurement stress,
mixed-operation stress, and a two-hour bounded soak.

## Earlier HWCDC Runs Kept As Diagnostic Evidence

- `2026-06-29`, `COM5`, requested 16 h: main suite passed (`69/0/0/0`), soak
  failed after `45.7` s with `16/3/0` and three consecutive prompt/resync
  failures around `settings`, `drv`, and `probe`.
- `2026-06-30`, `COM5`, requested 2 h: main suite passed (`69/0/0/0`), soak
  failed after `97.1` s with `107/1/0`; `stress_mix 5` reported internal
  `ok=30 fail=0` but the host did not receive the final prompt before timeout.
- `2026-06-30`, `COM5`, 3 minute preflight: passed (`339/0/0` soak outcomes),
  which was too short to prove the HWCDC long-run path.
- `2026-07-01`, `COM30`, TinyUSB CDC, 1 minute smoke: passed (`113/0/0` soak
  outcomes) before the two-hour release run.

## Limitations

- No electrical fault injection, disconnect testing, or unsafe stimulus was run.
- Raw register writes are intentionally excluded from the default HIL suite.
- Fixture-specific wiring and load plausibility remain manual checks.
- The final HIL run validates the Arduino ESP32-S3 TinyUSB CDC example path, not
  a pure ESP-IDF firmware build.
