# Hardware-in-the-loop validation

<a href="README.md">Documentation index</a> |
<a href="../examples/01_basic_bringup_cli/README.md">Arduino example</a>

This guide defines the maintained hardware qualification workflow for the
Arduino/PlatformIO diagnostic firmware. Build success and native tests do not
substitute for this run: HIL talks to a physical INA3221 through the public
example command surface and retains a reviewable report.

## Qualified stack and fixture

The repository exact-pins pioarduino `platform-espressif32` `55.03.311`, which
contains Arduino-ESP32 `3.3.11`, ESP-IDF `5.5.5`, esptool `5.3.0`, and the GCC
14.2.0 toolchain. The ESP32-S3 environment assumes 4 MB QIO flash, 2 MB QSPI
PSRAM, USB Serial/JTAG HWCDC, SDA on GPIO 8, SCL on GPIO 9, and an INA3221 at
`0x40`. Adjust `platformio.ini` and `examples/common/BoardConfig.h` before
claiming evidence for different hardware.

The native ESP-IDF example is a separate build surface qualified with ESP-IDF
`6.0.1`; an Arduino HIL result does not prove native-IDF hardware behavior.

## Prerequisites

- An isolated ESP32-S2/S3 fixture wired to a powered INA3221 with suitable I2C
  pull-ups and safe shunts/loads.
- PlatformIO Core `6.1.19` or newer.
- Python 3 and the `pyserial` package (`python -m pip install pyserial`).
- Exclusive access to the selected serial port.

On Windows, enable Win32 long-path support before the first framework install
or place `PLATFORMIO_CORE_DIR` behind a short drive/path. The Arduino `3.3.11`
library archive contains paths that can exceed the legacy 260-character limit.

Confirm the A0 strap, voltage levels, shunt ratings, and load limits before
uploading. The automated run changes volatile INA3221 configuration and alert
limit registers, then verifies their restoration. The chip has no EEPROM/NVM.

## Build, upload, and run

From the repository root, use a clean S3 build and the actual fixture port:

```powershell
.\scripts\pio.cmd run -e esp32s3dev -t clean
.\scripts\pio.cmd run -e esp32s3dev
.\scripts\pio.cmd run -e esp32s3dev -t upload --upload-port COM26
python tools/hil_cli_runner.py --parser-self-test
python tools/hil_cli_runner.py --port COM26 `
  --stress-count 500 --stress-mix-count 500 `
  --sample-benchmark --benchmark-count 500 `
  --soak-hours 2 --soak-failure-limit 1 --no-soak-transcript `
  --markdown-report ..\INA3221-hil-evidence\qualification.md `
  --transcript-file ..\INA3221-hil-evidence\qualification.transcript.txt
```

Replace `COM26` and the external artifact paths as needed. Reports and raw
transcripts are run artifacts, so keep them outside this repository. DTR and
RTS remain deasserted unless explicitly requested.

The default suite currently contains 379 bounded steps; `--sample-benchmark`
adds three more. Every ordinary step is enclosed by a sequence-specific
`HIL_BEGIN`/`HIL_END` frame, so an asynchronous prompt cannot be mistaken for
command completion. It covers:

- identity on the fixture's physical `0x40` strap, valid address-selection
  inputs for `0x40`-`0x43`, runtime selection/init/end,
  10/100/400 kHz switching and input-bound validation, aliases, cache certainty, and
  health;
- every cooperative job kind, symbolic progress/deadlines, explicit zero/one
  transfer budgets, automatic servicing, retained results/samples/alerts, and
  bus-silent cancellation;
- all eight public operating modes, including the alternate power-down
  encoding, and conversion-ready polling;
- all eight averaging, bus-conversion-time, and shunt-conversion-time values,
  with exact configuration-register readback;
- channel enable validation and voltage/current/power reads on all channels;
- critical/warning limits on all channels, summation selection/limit,
  power-valid limits, latch modes, and restore paths;
- software reset, managed/raw register guards, an intentionally mismatched raw
  alert limit with exact register/expected/actual/mask evidence and verified
  restoration, conversion helpers, transport counters, HIL framing, and invalid
  inputs;
- cache-only aggregate diagnostics, complete desired-profile output and host
  current direction, self-test, exact-count measurement/mixed/owner/frequency
  stress, optional benchmarks, and repeated soak health checks.

Every successful qualification must end `READY`, online, with zero consecutive
and total transport failures and clean hardware-configuration certainty. A
PASS count alone is insufficient; review the final health transcript and the
runner's limitations section.

Restoration is guaranteed only when the bounded suite reaches its restore and
final-health steps. If the host, serial link, or runner is interrupted, run
`recover` and verify `settings`, or power-cycle the INA3221, before reusing the
fixture.

## Evidence ledger

The maintained ledger records only reviewed summaries. Raw generated artifacts
remain external.

| Date | Target and port | Stack | Result | Evidence |
|---|---|---|---|---|
| 2026-08-05 | ESP32-S3 rev 0.1, COM26, 4 MB QIO flash, 2 MB QSPI PSRAM | INA3221 `3.1.0`; pioarduino `55.03.311`; Arduino `3.3.11`; IDF `5.5.5`; PlatformIO `6.1.19` | PASS: 379/379 bounded checks; 50/50 measurement stress; 300/300 mixed operations; 25/25 owner jobs; 20/20 frequency switches; post-run `READY`, online, 2,212 successes, zero failures, last error never, configuration verified | External `release-short-hil.md` SHA-256 `716A4E6416F83AE7BC2F0AEFDDE1E94E20F3B4EABAA8C23589E7E81076E7D874`; transcript SHA-256 `7DCBAB18DCB3A09C2C79B4DD32B803D327BFC20156BD37C1BC6BEF97B5B4FF11`; release-candidate worktree based on `afe1c1b8702e90fed4e5ca5f9d81696362cbff92` |
| 2026-07-31 | ESP32-S3 rev 0.1, COM26, 4 MB QIO flash, 2 MB QSPI PSRAM | pioarduino `55.03.311`; Arduino `3.3.11`; IDF `5.5.5`; PlatformIO `6.1.19` | PASS: 289/289 feature checks; 500/500 measurement stress; 3,000/3,000 mixed operations; 13,540/13,540 soak commands over 7,200.0 s; post-run `READY`, online, 162,915 successes, zero failures, configuration verified | External `qualification-2h-final-pass.md` SHA-256 `5CCD2CD0E420A2BF1EAD4D48AE89D8804D5590F76397180CB41F39C5A984EEAE`; transcript SHA-256 `AAFDB5FD95EF4B02C59587D4123A1835C4366C86FB057A2D4843ADE694449AFF`; source worktree was dirty at `7d15782259f786ef11ee923ba57447881b258873` |

## Validation boundary

The automated command suite does not by itself prove:

- measurement accuracy against a calibrated voltage/current reference;
- electrical assertion, polarity, timing, and read-clear behavior at the
  Critical, Warning, PV, or TC pins;
- behavior during an externally induced NACK, stuck bus, disconnect, or power
  fault;
- shunt thermal safety or load-transient behavior;
- ESP32-S2 hardware or native ESP-IDF hardware operation.

Those require controlled bench procedures and must be reported separately.
Never perform destructive fault injection on production hardware.
