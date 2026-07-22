# INA3221 HIL and TunnelMonitor Validation Summary - 2026-07-22

This report condenses the release-relevant evidence from the one-hour hardware
campaign, exact-final firmware qualification, host tests, and a read-only
TunnelMonitor-node contract harness. Raw serial transcripts and generated
runner reports remain outside the repository.

## Exact basis

- INA3221 branch: `hardening/tunnelmonitor-suitability-reaudit`
- INA3221 base commit: `e608a49471e9e5dfe9708de15fda3142456261a2`
- Library version: `3.0.0` plus the uncommitted fixes described below
- TunnelMonitor-node inspected baseline: commit
  `4d7555a2306b38032d7f6cbb15ccb29674fcecca` on
  `prompt-45-platformization`
- Host: Windows 11 `10.0.26200`, Python `3.12.10`, PlatformIO `6.1.19`
- Board: ESP32-S3, TinyUSB CDC, INA3221 at `0x40`
- Identity: Manufacturer ID `0x5449`, Die ID `0x3220`
- The board exposes the same ESP32-S3 (`VID:PID 303A:1001`, serial
  `64:E8:33:73:A1:54`) as bootloader/upload endpoint `COM26` and runtime CDC
  endpoint `COM30`. The requested COM26 board was therefore exercised through
  COM30 after each upload.

The one-hour firmware already contained the corrected derived `readBlocking()`
deadline. A later source-level cleanup changed the public default-selection
form to a named sentinel and removed two unused private helpers without changing
that runtime behavior. The exact-final source was then rebuilt, uploaded, and
qualified twice after the soak. The uploaded final firmware SHA-256 was
`0E27BCCBB7B70495347F4CD0D2B61E380CD00AFE8AD951A32C8612EBBF387459`.

## Findings and fixes

### Default blocking deadline

The first hardware suite exposed a hard compatibility defect. The fixed 200 ms
default for `readBlocking()` could not pass the v3 bus-silent admission check
for a healthy three-channel triggered sample whose successful path permits
eight callbacks at the configured 50 ms callback ceiling, plus conversion and
scheduling margin. The command correctly returned `DEADLINE_EXPIRED`, but the
default API could not work with the example's valid default transport bound.

The default now derives one finite timeout from the active profile, datasheet
maximum conversion time, successful-path transfer count, callback ceiling, and
the existing compatibility scheduling margin. Explicit non-zero timeouts retain
their previous bounded behavior. Poll-count arithmetic is 64-bit, and native
regressions cover both triggered and continuous modes with a 50 ms callback
ceiling.

### Validation workflow correctness

- Direct compatibility setters intentionally make profile certainty `DIRTY`.
  The HIL workflow now calls `recover()` before subsequent direct measurements
  instead of weakening the library's verified-profile precondition.
- Benchmark-enabled suites now compose the final health query after, rather
  than before, all benchmark work.
- The default HIL suite was expanded to every operating mode, all eight
  averaging values, all eight bus and shunt conversion-time values, all three
  channel enable/calibration paths, every alert-register family with
  write/readback/restore, identity/raw reads, recovery/reset, invalid inputs,
  stress, and benchmarks.
- Two stale dormant compatibility tests were refactored to the v3 atomic and
  profile-certainty contracts, all public functions now execute in native
  coverage, and two genuinely unused private helpers were removed.

## Hardware results

### One-hour campaign

- Functional pre-suite: `71/71` passed
- Soak interval: `2026-07-22T19:14:25+02:00` through
  `2026-07-22T20:14:25+02:00`
- Exact requested duration: `3600.0` s
- Soak outcomes: `6769/0/0` pass/fail/unknown
- Maximum consecutive failures: `0`
- Worst command latency: `0.047` s
- Worst measurement-read latency: `0.032` s
- Manual recovery commands: `423`, all successful
- Stop reason: completed requested duration
- Command counts: 847 aggregate reads; 846 each raw-shunt, raw-bus, current,
  and power reads; 423 each readiness, mixed-stress, settings, health, probe,
  and recovery operations
- Pre-soak stress: 500/500 measurements and 3000/3000 mixed operations passed
- Pre-soak benchmarks: 500 aggregate samples, 500/500 measurement stress, and
  3000/3000 mixed operations passed

### Exact-final qualification

The final uploaded firmware passed the expanded suite twice. The authoritative
ordered repeat was `152/152` passed with no failures, unknowns, or skipped
steps. It included:

- all seven operating modes and triggered-ready polling;
- every averaging and conversion-time enum value;
- three-channel enable, calibration, measurement, alert-limit, summation, and
  latch paths with restoration;
- 1000/1000 measurement stress operations;
- 6000/6000 mixed operations, including 1000 successful `readBlocking()` calls;
- a 1000-sample aggregate benchmark;
- a second 1000/1000 measurement benchmark and 6000/6000 mixed benchmark;
- final driver state `READY`, 80,392 total successes, zero total failures, zero
  consecutive failures, and no last error.

The observed aggregate fixture values were consistent across all three
channels: 0.200 mV shunt, 3.960 V bus, 2 mA current, and 8 mW fixed-unit power.

## Host, build, and coverage results

- Native Unity: `122/122` passed
- GCC coverage of `src/INA3221.cpp`: 85.98% lines executed, 94.12% branch sites
  executed, 65.16% branch directions taken, 76.87% calls executed, and zero
  uncalled functions
- Exhaustive raw-domain properties: 327,680 checks passed across all 65,536
  16-bit patterns for signed 13-bit shunt/bus decode and re-encode plus signed
  15-bit shunt-sum encoding
- The native timing test exhausts all conversion-time, averaging, enabled-channel,
  and applicable mode combinations and uses datasheet maximum timing
- Strict framework-neutral C++17 warning-as-error compile: passed
- GCC `-fanalyzer`: passed
- Cppcheck: zero high and zero medium findings; remaining findings are low-level
  style/model noise
- CLI, core timing, native-IDF source contract, version, and metadata guards:
  passed
- HIL parser self-test, unique step-ID check, and final-step ordering check:
  passed
- Doxygen warnings-as-errors: passed
- PlatformIO package creation: passed (`256,717` bytes)
- ESP32-S3 Arduino build: 42,464/327,680 bytes RAM and
  449,142/1,310,720 bytes flash
- ESP32-S2 Arduino build: 37,712/327,680 bytes RAM and
  400,781/1,310,720 bytes flash
- `idf.py` was not available on this host, so a local native ESP-IDF compile is
  not claimed; the native-IDF source contract guard passed.

The core/public source remains framework-neutral, log-free, allocation-free,
and free of `delay()` calls.

## TunnelMonitor-node behavior

The inspected committed TunnelMonitor-node baseline passed `1109/1109` native
tests. Its `tunnelmonitor_wifi` build passed at 179,720/327,680 bytes RAM and
1,803,570/8,323,072 bytes flash.

A strict C++17 warning-as-error harness compiled the final INA3221 source
against the real TunnelMonitor headers and exercised the expected owner-private
adapter boundary. It verified:

- exactly one physical backend call per owner poll and exact byte-count
  completion;
- TunnelMonitor's 20 ms transfer ceiling and 1000 ms power-read deadline;
- bounded initialization (`<=35` transfers) and triggered sampling (`<=8`);
- atomic three-channel fixed-unit results and bus-silent cancellation;
- bus-silent rejection of a deadline that cannot fit;
- typed NACK/absent/timeout/bus error mapping with backend detail retained;
- passive OFFLINE telemetry followed by owner-requested manual initialization
  recovery to READY;
- projection of a selected primary channel to the current scalar power result:
  4000 mV, 200 uV, 2 mA, 8 mW, with die-temperature validity clear.

The current approved `tunnelmonitor_s3_hw200` profile remains an INA228 at
`0x41`; its device kind, UI label, configuration, and reading catalog are
INA228-specific. Installing INA3221 into that profile would silently discard
two channels and mislabel the device. No TunnelMonitor source was changed.
A future INA3221 profile still needs explicit approval of address strap,
channel meanings, shunt calibration/direction, alert and sampling policy, and
scalar-versus-three-channel result schema. The suitable integration is one
owner-private adapter using the production `bind()`/job API, never a parallel
bus owner or an INA228 alias.

TunnelMonitor-node was clean at initial inspection. Concurrent user/other-agent
changes appeared there during this campaign, including new generic I2C owner
interfaces. They were not edited or reverted. The committed-baseline tests and
build above predate those changes; the strict adapter harness was recompiled
successfully against the later exact-byte-count header shape.

## Limitations

- Electrical fault injection, live disconnect, alert-pin stimulus, deliberate
  bus-stuck injection, and unsafe raw register writes were not performed on the
  connected fixture. Deterministic native fault injection covers the software
  paths without risking shared hardware.
- The fixed fixture cannot physically validate all four INA3221 strap addresses,
  negative current direction, alternate shunt values, or every alert threshold;
  native/exhaustive tests and safe write/readback/restore cover those contracts.
- The one-hour run validates the ESP32-S3 Arduino TinyUSB example transport.
  It is not native ESP-IDF or a future TunnelMonitor production-board HIL run.
