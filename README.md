# INA3221 Driver Library

Framework-neutral, production-oriented INA3221 triple-channel voltage/current
monitor driver for ESP32-S2 and ESP32-S3 projects using Arduino, PlatformIO, or
native ESP-IDF.

Library version: `v3.1.0`

Version 3.1 adds complete Arduino/native-IDF diagnostic CLI parity, retained
register-mismatch evidence, runtime address and I2C-frequency control, bounded
self/stress tests, and sequence-framed HIL automation.

The v3 production API separates application-owned I2C transport from a complete
device profile and executes hardware work through one cooperative,
deadline-aware job engine. The core does not own or configure the bus, create a
task, take a lock, retry a transfer, recover a bus, log, or allocate in steady
operation.

## Features

- Three independently enabled shunt/bus-voltage channels.
- All INA3221 averaging settings, conversion times, triggered/continuous modes,
  software reset, identity checks, and power-down behavior.
- Per-channel critical and warning limits, shunt summation, power-valid window,
  latch policy, CVRF readiness, and retained destructive alert evidence.
- Correct signed 13-bit decoding with 40 µV shunt and 8 mV bus LSBs.
- Host-side fixed-unit current and power calculation with explicit shunt
  resistance and current-direction calibration.
- Strict per-poll transfer budgets, absolute deadlines, take-once results,
  configuration certainty, and passive transport-health telemetry.
- No Arduino or ESP-IDF headers in public/core library code; application
  adapters provide the actual I2C callbacks.
- Bounded synchronous compatibility API for standalone bring-up and diagnostics.

The library requires C++17. This repository validates ESP32-S2 and ESP32-S3;
other targets are not claimed by the package metadata.

## Hardware and address selection

The library never selects SDA/SCL pins, bus frequency, pull-ups, or controller
timeouts. Configure those in the application. `DeviceProfile::i2cAddress` is a
seven-bit address determined by the INA3221 A0 connection:

| A0 connection | Address |
|---|---:|
| GND | `0x40` |
| VS | `0x41` |
| SDA | `0x42` |
| SCL | `0x43` |

Use Kelvin connections at each shunt and choose a resistance/power rating that
keeps the expected shunt voltage inside the device range. The resistance in the
profile is host-side calibration; it does not program an INA3221 calibration
register.

## Production API

Use `TransportConfig`, `DeviceProfile`, and the cooperative owner methods for
new integrations.

### Transport contract

`TransportConfig` is non-owning. Both callbacks are required, and
`defaultTransferTimeoutMs` must be non-zero. A callback may reject unsupported
parameters locally with `INVALID_CONFIG`/`INVALID_PARAM`; otherwise it
represents exactly one synchronous physical attempt and must:

- transfer the exact requested lengths before returning `OK`;
- return no later than the supplied timeout;
- map backend failures to `Status` without leaking framework-specific types;
- avoid hidden retry, recovery, bus reconfiguration, or interleaving; and
- avoid calling back into the same `INA3221` object.

`nowMs` drives legacy blocking helpers and the health timestamps exposed as
`lastOkMs()` / `lastErrorMs()`; `cooperativeYield` is used only by blocking
compatibility work. The production job API receives 64-bit monotonic time
directly through `PollContext`.

The driver records whether it invoked a callback before classifying an owner
deadline failure or possible write effect. Callback-local validation should
return `INVALID_CONFIG` or `INVALID_PARAM`; actual bus failures should use the
most accurate transport error available. In particular, a deadline that rounds
to zero inside the driver is bus-silent and cannot make a write indeterminate.

### Complete device profile

`DeviceProfile` is the desired state of every managed volatile register plus
host calibration. A default-constructed profile intentionally has zero shunt
resistance: set `resistanceMicroOhms` for every enabled channel before `bind()`.

Important validation rules include:

- address must be `0x40` through `0x43`;
- channel masks may use only bits 0 through 2;
- every enabled channel needs a non-zero shunt calibration;
- a non-power-down mode needs at least one enabled channel;
- summation channels must be a subset of enabled channels;
- alert limits must fit their register formats; and
- the power-valid lower limit must be below the upper limit, with both no
  greater than 26,000 mV.

The initial defaults are:

| Profile field | Default |
|---|---|
| Enabled channels | CH1, CH2, CH3 |
| Averaging | 1 sample |
| Bus/shunt conversion time | 1.1 ms each |
| Mode | Continuous shunt + bus |
| Shunt resistance | 0 µΩ; must be configured for enabled channels |
| Critical/warning limits | 163,800 µV per channel |
| Summation channels | None |
| Shunt-sum limit | 655,320 µV |
| Power-valid window | 9,000–10,000 mV |
| Warning/critical latch | Disabled |

### Job lifecycle

The normal lifecycle is:

1. `bind()` validates and stores the transport/profile without I2C.
2. `startInitialize()` admits identity verification and full profile
   reconciliation.
3. The bus owner calls `pollJob()` from one serialized, non-ISR context.
4. Cache-only `getJobProgress()` reports the current stage and whether a result
   is pending.
5. `takeJobResult()` consumes every terminal result exactly once.
6. Start sample, profile, reconcile, or power-down jobs under the same policy.
7. Finish `startPowerDown()` when required, then call bus-silent `unbind()`.

A successful power-down replaces the retained desired mode with `POWER_DOWN`.
`startReconcile()` and `recover()` consequently keep the device powered down;
wake it by applying a profile with a measurement mode.

Every start call requires a non-zero request ID and a non-zero absolute deadline
in the same monotonic millisecond domain as `PollContext::nowMs`. `IN_PROGRESS`
means admission, not completion. A pending result blocks admission of another
job, including after cancellation or failure.

`PollContext::deadlineMs` may be zero to use the job deadline or may shorten it.
`transferTimeoutMs` may be zero to use the configured default or may shorten it.
Set `maxTransfers` to at least one to make progress; a shared-bus owner normally
uses exactly one.

### Minimal owner loop

The transport functions below are application adapters. See the complete
<a href="examples/01_basic_bringup_cli/README.md">Arduino example</a> and
<a href="examples/esp_idf/basic/README.md">native ESP-IDF example</a> for real
single-attempt implementations.

```cpp
#include "INA3221/INA3221.h"

INA3221::INA3221 monitor;
uint32_t nextRequestId = 1;

INA3221::TransportConfig makeTransport() {
  INA3221::TransportConfig transport{};
  transport.i2cWrite = appI2cWrite;
  transport.i2cWriteRead = appI2cWriteRead;
  transport.i2cUser = &applicationBus;
  transport.defaultTransferTimeoutMs = 20;
  transport.offlineThreshold = 5;  // Passive telemetry only.
  return transport;
}

INA3221::DeviceProfile makeProfile() {
  INA3221::DeviceProfile profile{};
  profile.i2cAddress = 0x40;
  profile.enabledChannels = INA3221::ALL_CHANNELS;
  profile.mode = INA3221::Mode::SHUNT_BUS_TRIG;
  for (auto& shunt : profile.shunts) {
    shunt.resistanceMicroOhms = 100000;  // 100 mΩ.
  }
  return profile;
}

void start(uint64_t nowMs) {
  INA3221::Status status = monitor.bind(makeTransport(), makeProfile());
  if (!status.ok()) return;

  status = monitor.startInitialize(nextRequestId++, nowMs + 1000);
  if (!status.inProgress()) {
    monitor.unbind();
  }
}

void service(uint64_t nowMs) {
  INA3221::JobProgress progress{};
  (void)monitor.getJobProgress(progress);

  if (!progress.resultPending && progress.state == INA3221::JobTerminalState::ACTIVE) {
    INA3221::PollContext poll{};
    poll.nowMs = nowMs;
    poll.deadlineMs = progress.deadlineMs;
    poll.transferTimeoutMs = 20;
    poll.maxTransfers = 1;
    (void)monitor.pollJob(poll);
    (void)monitor.getJobProgress(progress);
  }

  if (!progress.resultPending) return;

  INA3221::JobResult result{};
  if (!monitor.takeJobResult(result).ok()) return;
  if (!result.status.ok()) {
    // Inspect result.state and result.hardwareEffect before retry/reconcile.
    return;
  }
  if (result.sampleValid) {
    consume(result.sample);
  }
}
```

After successful initialization, use
`startTriggeredSample(Mode::SHUNT_BUS_TRIG, id, deadline)` for an atomic
triggered batch, or `startContinuousSample(id, deadline, consumeAlerts)` for a
mixed-age continuous read.

## Samples, units, and provenance

`SampleBatch` contains exactly three fixed channel slots. Always inspect
`enabledChannels`, `validChannels`, and each reading's `validQuantities`; zero is
a valid measurement and is not itself a validity signal.

| Field | Unit | Source |
|---|---|---|
| `shuntMicroVolts` | µV | Signed 13-bit shunt register |
| `busMilliVolts` | mV | Bus register; physical range checked through validity |
| `currentMilliAmps` | mA | Shunt voltage / calibrated resistance |
| `powerMilliWatts` | mW | Bus voltage × current |

Each committed batch also carries coherence, capture uptime, request ID, and
profile generation. Triggered samples use `TRIGGERED_ATOMIC`; continuous reads
use `CONTINUOUS_MIXED_AGE` because channel registers are read sequentially.
Partial work is never published as the last-good sample, and `peekLastSample()`
performs no I2C.

## Alert evidence and configuration certainty

Reading Mask/Enable is destructive for CVRF and latched CF/SF/WF events. Every
library read retains those event bits before exposing the current value.
`peekAlertEvents()` is non-consuming; `takeAlertEvents()` acknowledges the
retained events after copying them. `AlertSnapshot` separately exposes the raw
read, writable SCC/WEN/CEN bits, condition-level PVF, the latest raw TCF level,
a derived `timingControlFault` condition (TCF low), CVRF, and
`evidenceUncertain`.

The timing-control function is enabled by the INA3221 power-up/reset sequence.
A Configuration-register write before that sequence completes disables the
function until the next power cycle or reset; applications that depend on TC
must account for when initialization writes occur.

If a failed or short transport attempt may already have reached a destructive
read, `evidenceUncertain` remains set until the application takes the retained
record. The library never fabricates alert evidence that could have been lost.

Every terminal `JobResult` includes its `HardwareEffect`. Inspect that together
with `measurementConfigState()` and `alertConfigState()`:

| State | Meaning | Owner action |
|---|---|---|
| `APPLIED` | Desired managed state was read back and verified | Measurement may be admitted |
| `DIRTY` | A confirmed side effect changed managed state | Reconcile before measurement |
| `UNKNOWN` | Hardware acceptance or retained state is ambiguous | Reconcile or fully initialize |

`HardwareEffect::PARTIAL` and `INDETERMINATE` deliberately avoid claiming
rollback after confirmed or ambiguous writes.

Profile application is verified register by register, but is not an atomic
hardware transaction. A live apply can transiently expose the newly written
Configuration register alongside older alert limits until the job completes;
admit it only under application policy appropriate for the connected load.

When profile or power-down readback fails, `JobResult::mismatchValid` is true
and `mismatchRegister`, `mismatchExpected`, `mismatchActual`, and
`mismatchMask` retain the first exact comparison failure. These fields are
zero/false for results that did not fail register verification.

## Deterministic bounds

`maximumJobTransfers()` returns the successful-path ceiling for a valid profile
when triggered CVRF is high on its first eligible check. With all three channels
enabled:

| Job | Maximum callbacks |
|---|---:|
| `INITIALIZE` | 35 |
| `APPLY_PROFILE` | 33 |
| `RECONCILE` | 33 |
| `TRIGGERED_SAMPLE` | 8 |
| `CONTINUOUS_SAMPLE` | 7 |
| `POWER_DOWN` | 3 |

For `N` enabled channels, triggered sampling is bounded by `2 + 2N` on this
success path and continuous sampling by `1 + 2N`. Profile jobs conservatively
count read, optional write, and readback verification for every managed
register; already-matching profiles use fewer calls.

If triggered CVRF is low at the first eligible check, the engine retains the
observed alert bits and uses a fixed 50 ms fault-recheck interval. If another
bounded read cannot fit, it remains bus-silent until the absolute deadline.
The fault-path read count is therefore bounded by that deadline, not by the
eight-callback success figure.

With `maxTransfers = 1`, one `pollJob()` call performs at most one synchronous
callback. For a larger budget, the driver divides the remaining effective
deadline across the requested transfers and caps each callback timeout. If the
share rounds to zero, the job terminalizes with `DEADLINE_EXPIRED` without
starting another transfer.

### Conversion timing

`conversionTiming()` exposes datasheet-typical and driver scheduling-maximum
values for one conversion:

| Setting | Typical (µs) | Maximum (µs) |
|---|---:|---:|
| `CT_140US` | 140 | 154 |
| `CT_204US` | 204 | 224 |
| `CT_332US` | 332 | 365 |
| `CT_588US` | 588 | 646 |
| `CT_1100US` | 1,100 | 1,210 |
| `CT_2116US` | 2,116 | 2,328 |
| `CT_4156US` | 4,156 | 4,572 |
| `CT_8244US` | 8,244 | 9,068 |

`maximumCycleTimeUs()` multiplies the applicable shunt/bus maximum by enabled
channel count and averaging sample count. After the trigger callback returns, a
strictly later poll timestamp becomes the conversion-wait origin. The job waits
the maximum cycle plus a fixed 100 µs wake margin, rounded up to milliseconds,
before its first CVRF/alert read.

Before writing the trigger, the driver rejects a deadline that cannot contain
the one-tick origin advance, maximum conversion wait, and successful-path
callback bounds. For the largest three-channel shunt+bus, 1024-sample profile,
the typical cycle is 50,651,136 µs and the scheduling maximum is 55,713,792 µs
before the wake margin. A 1,000 ms deadline is rejected before I2C.

INA3221 registers are volatile and the device has no EEPROM/NVM. There are no
hidden erase, program, or persistence waits.

## Error and health model

All fallible APIs return `Status` with a stable `Err`, an optional numeric
detail, and a static-lifetime message. Transport adapters should preserve a
useful backend code in `detail`. Validation and precondition failures do not
count as transport-health failures.

Tracked initialization transfers do count before `begin()` succeeds, so a
failed first bring-up still preserves `lastError`, timestamps, and counters
while `DriverState` remains `UNINIT`.

`READY`, `DEGRADED`, and `OFFLINE` are passive diagnostics only. `OFFLINE` never
suppresses an admitted transfer; the application owns admission, backoff,
retry, and recovery policy. A tracked success returns health to `READY` and
clears consecutive failures. Totals saturate and remain object-lifetime
diagnostics across bind/unbind cycles.

Do not automatically retry `PARTIAL` or `INDETERMINATE` writes as though no
hardware change occurred. Take the result, inspect certainty/effect, and run
`startReconcile()` or `startInitialize()` under application policy.

## Ownership and concurrency

- One application context owns each object and its bus admission.
- Serialize every method, cache access, and transport callback in that context.
- The class is non-copyable, non-movable, non-reentrant, and not ISR-safe.
- Do not call any driver method from a transport callback.
- The driver owns no I2C instance, task, mutex, queue, retry/recovery policy,
  scheduling policy, or deadline renewal.
- No public method is safe to race with `pollJob()`, `cancelJob()`, `unbind()`,
  or result/cache access.

## Legacy synchronous compatibility

`Config`, `begin()`, direct getters/setters, raw register access,
`readBlocking()`, `probe()`, and `recover()` remain bounded compatibility tools
for standalone bring-up and diagnostics. They are not the recommended
shared-I2C-owner steady path.

| Compatibility operation | Worst-case transfer behavior |
|---|---|
| `begin()` / `recover()` | Up to 35 synchronous callbacks in one call |
| `probe()` | Two synchronous raw identity reads; no health update |
| Direct raw/scaled read | One callback in continuous mode; triggered reads also consume Mask/Enable readiness, while `readChannel()` and `readPower()` use multiple data reads |
| Typed hardware setter | Two callbacks: write plus verification readback |
| `powerDown()` | Up to three callbacks for read/write/verify |
| `readBlocking()` | Budget-one internal polling plus bounded cooperative polls until timeout |
| Legacy staged APIs | Caller-supplied instruction budget and a derived finite deadline |

Each callback still has its configured timeout, so a multi-transfer synchronous
call can block for the sum of callback bounds plus local work. Compatibility
calls reject an active production job. Production sample jobs reject a legacy
conversion in progress; lifecycle/recovery jobs abandon that legacy bookkeeping
bus-silently because they rewrite Configuration. The `cancelConversion()`
compatibility method provides an explicit bus-silent escape. Diagnostic writes
can make profile certainty `DIRTY` or `UNKNOWN`; reconcile before returning to
the production engine.

The staged compatibility methods extend their 32-bit monotonic time input
through one wrap for the active job; callers must poll at least once per 32-bit
clock period. `readBlocking()` uses wrap-safe elapsed time, publishes capture
uptime in the caller's absolute monotonic domain, and has a stalled-clock spin
guard that resets on time or transfer progress. When its timeout is
omitted, it derives a finite bound from the verified profile, maximum conversion
time, successful-path transfer count, and configured per-transfer timeout. An
explicit timeout remains available when the application needs a tighter bound.
Prefer the explicit 64-bit production API for new code.

## Installation and integration

### PlatformIO / Arduino

Pin a release tag:

```ini
lib_deps =
  https://github.com/janhavelka/INA3221.git#v3.1.0
```

Then include the public umbrella header:

```cpp
#include "INA3221/INA3221.h"
```

The repository's Arduino example uses project-local glue under
`examples/common/`; that directory is not part of the library API. Adjust
`BoardConfig.h` for the actual board instead of copying its reference pins into
library code.

Repository example and HIL builds exact-pin pioarduino
[`platform-espressif32` `55.03.311`](https://github.com/pioarduino/platform-espressif32/releases/tag/55.03.311),
which supplies Arduino-ESP32 `3.3.11` and ESP-IDF `5.5.5` and requires
PlatformIO Core `6.1.19` or newer. This replaces the previous `54.03.20` /
Arduino `3.2.0` / ESP-IDF `5.4.1` build baseline. Consuming applications own
their platform selection; the repository pin qualifies the examples, not an
application's separate toolchain.

### Native ESP-IDF

Add this repository as a component through `EXTRA_COMPONENT_DIRS`, a project
component checkout, or equivalent component-manager integration. The root
`CMakeLists.txt` exports `include/`, compiles `src/INA3221.cpp`, and requests
C++17. With a direct `EXTRA_COMPONENT_DIRS` integration, ESP-IDF derives the
component name from the checkout directory: keep that leaf directory named
`INA3221` when consumers declare `REQUIRES INA3221`, or provide an equivalent
fixed-name wrapper. The native example already uses such an example-local
component shim, so its build does not depend on the checkout directory name.
It is independently compiled with ESP-IDF `6.0.1` and
`driver/i2c_master.h`; that is separate from IDF `5.5.5` bundled inside the
Arduino build above.

See the [native ESP-IDF integration guide](docs/IDF_PORT.md).

## Examples

- <a href="examples/01_basic_bringup_cli/README.md">Arduino/PlatformIO bring-up CLI</a>:
  complete cooperative job control, address/frequency lifecycle, all chip
  setters, retained sample/alert/register-mismatch evidence, exact transfer
  counters, full diagnostics, framed HIL, stress, and self-test commands.
- <a href="examples/esp_idf/basic/README.md">Native ESP-IDF application</a>: native
  `app_main`, `driver/i2c_master.h`, `esp_timer`, FreeRTOS waits, fixed C
  buffers, and the same owner-safe workflow without Arduino facades.
- [`examples/common/`](examples/common/): Arduino-example glue only, not
  installed library code.

## Validation

The authoritative local validation matrix is in [CONTRIBUTING.md](CONTRIBUTING.md).
For a quick build smoke check, run the native tests and both Arduino target
builds from the root:

```powershell
.\scripts\pio.cmd test -e native
.\scripts\pio.cmd run -e esp32s3dev
.\scripts\pio.cmd run -e esp32s2dev
```

CI compiles native tests, both Arduino targets, and both native ESP-IDF targets.
A static source-contract check is not an ESP-IDF compile. When `idf.py` is not
available, report the corresponding local build as not run.

The release-candidate Arduino fixture is covered by the reviewed 379-step
short-HIL result in [docs/HIL.md](docs/HIL.md). That run is intentionally
separate from native-IDF and ESP32-S2 hardware claims.

Doxygen HTML is generated at `docs/doxygen/html/index.html` and is intentionally
ignored by Git. Public headers are checked for undocumented symbols and missing
parameter documentation with warnings treated as errors.

## Documentation map

- <a href="docs/README.md">Documentation index</a> — maintained guides and the
  bundled device datasheet.
- [Changelog](CHANGELOG.md) — release history and unreleased changes.
- [Public API header](include/INA3221/INA3221.h) — authoritative Doxygen contract.
- [Native ESP-IDF integration](docs/IDF_PORT.md) — bus ownership and adapter rules.
- [Hardware-in-the-loop validation](docs/HIL.md) — reproducible fixture run and
  reviewed evidence ledger.
- [Code audit resolution](docs/CODE_AUDIT_RESOLUTION.md) records the
  finding-by-finding verification and selected remedies for the 2026-08-27 audit.
- <a href="docs/INA3221_datasheet.pdf">INA3221 datasheet</a> — authoritative
  bundled device reference.

## License

MIT License. See [LICENSE](LICENSE).
