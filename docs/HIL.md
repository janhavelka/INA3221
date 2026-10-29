# Hardware-in-the-loop validation

[Documentation index](https://github.com/janhavelka/INA3221/blob/main/docs/README.md) |
[Arduino example](https://github.com/janhavelka/INA3221/blob/main/examples/01_basic_bringup_cli/README.md)

This guide defines the maintained hardware qualification workflow for the
Arduino/PlatformIO diagnostic firmware. Build success and native tests do not
substitute for this run: HIL talks to a physical INA3221 through the public
example command surface and retains a reviewable report.

## Configured stack and reference fixture

The repository exact-pins pioarduino `platform-espressif32` `55.03.311`, which
contains Arduino-ESP32 `3.3.11`, ESP-IDF `5.5.5`, esptool `5.3.0`, and the GCC
14.2.0 toolchain. The ESP32-S3 environment assumes 4 MB QIO flash, 2 MB QSPI
PSRAM, USB Serial/JTAG HWCDC, SDA on GPIO 8, SCL on GPIO 9, and an INA3221 at
`0x40`. The runner checks those framework versions and memory sizes, and
expects exactly one healthy INA3221 at `0x40`. Different memory layouts,
addresses or target fixtures require reviewed runner expectations as well as
changes to `platformio.ini` and `examples/common/BoardConfig.h` before claiming
hardware evidence. The firmware build targets remain ESP32-S2 and ESP32-S3.

The native ESP-IDF example is a separate build surface targeted by CI with ESP-IDF
`6.0.1`; an Arduino HIL result does not prove native-IDF hardware behavior.

## Prerequisites

- An isolated reference fixture matching the expectations above, wired to a
  powered INA3221 with suitable I2C pull-ups and safe shunts/loads.
- PlatformIO Core `6.1.19` or newer.
- Python 3 and the `pyserial` package (`python -m pip install pyserial`).
- Exclusive access to the selected serial port.

On Windows, enable Win32 long-path support before the first framework install
or place `PLATFORMIO_CORE_DIR` behind a short drive/path. The Arduino `3.3.11`
library archive contains paths that can exceed the legacy 260-character limit.

Confirm the A0 strap, voltage levels, shunt ratings, and load limits before
uploading. The automated run changes volatile INA3221 configuration and alert
limit registers and checks restoration to the suite's fixed test baseline;
it does not preserve arbitrary pre-run settings. The chip has no EEPROM/NVM.

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

The default suite currently contains 404 bounded steps; `--sample-benchmark`
adds three more. Every ordinary step is enclosed by a sequence-specific
`HIL_BEGIN`/`HIL_END` frame, so an asynchronous prompt cannot be mistaken for
command completion. It covers:

- identity on the fixture's physical `0x40` strap, valid address-selection
  inputs for `0x40`-`0x43`, runtime selection/init/end,
  10/100/400 kHz switching and input-bound validation, aliases, cache certainty, and
  health;
- verified setters followed by measurement without recovery, exact two-callback
  Configuration and three-callback Mask/Enable setters, rejected unselected
  shunt-sum reads, and triggered blocking reads at AVG_1 and AVG_4;
- every cooperative job kind, symbolic progress/deadlines, explicit zero/one
  transfer budgets plus a Wire-safe requested budget of 255, automatic servicing,
  retained results/samples/alerts, and bus-silent owner and legacy cancellation;
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

Treat restoration as verified only when the restore, register-verification and
final-health checks pass. Reaching those steps alone is insufficient: ordinary
test failures do not stop the suite. After interruption or failure, reapply the
intended profile and verify it before reusing the fixture. `recover` reapplies
the currently retained profile, which may still be an intermediate test profile;
a power cycle restores chip defaults rather than the application's settings.

## Evidence ledger

The maintained ledger records only reviewed summaries. Raw generated artifacts
remain external.

The 2026-09-05 audit verification extended the suite and passed its host parser
checks. The entries below predate these changes; they do not constitute hardware
evidence for the audit fixes or the expanded 404-step suite.

**3.2.0 qualification status (2026-09-08): NOT RUN.** COM11 and COM12 were
enumerated as ESP USB serial interfaces, but neither was confirmed as an
isolated ESP32-S2/S3 fixture wired to an INA3221. Hardware testing stopped at
fixture identification; no firmware was uploaded, no HIL commands were sent,
and no transcript or result hash was generated. Host parser checks and green
CI do not qualify the hardware behavior of this release.

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
