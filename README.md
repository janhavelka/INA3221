# INA3221 Driver Library

Framework-neutral, production-oriented INA3221 triple-channel voltage/current
monitor driver for ESP32-S2 and ESP32-S3 projects using Arduino, PlatformIO, or
native ESP-IDF.

Library version: `v3.0.0`

The next major API separates transport ownership from the complete device
profile and provides one cooperative, deadline-aware job engine. The library
does not own or configure the I2C bus, create a task, take a lock, retry a
transfer, recover a bus, or allocate in steady operation.

## Production API

Use `TransportConfig` plus `DeviceProfile` for new integrations.

- `TransportConfig` is a non-owning callback contract. Each callback represents
  exactly one synchronous physical attempt, must transfer the exact lengths,
  must honor its supplied timeout, and must not retry, recover, reconfigure the
  bus, interleave another client, or re-enter the driver.
- `DeviceProfile` is the complete desired volatile state: address, channel
  mask, averaging, conversion times, mode, fixed-unit shunt calibration and
  direction, alert thresholds, summation selection, power-valid window, and
  latch policy.
- `bind()` validates and stores both contracts with zero I2C. `unbind()` is
  bus-silent and discards active/pending/cached state. Use `startPowerDown()` and
  finish its result before `unbind()` when the device must be powered down.
- `startInitialize()`, `startApplyProfile()`, and `startReconcile()` use the
  same staged read/write/verify engine. Initialization additionally verifies
  Manufacturer ID `0x5449` and Die ID `0x3220`.
- `pollJob(PollContext)` admits no more than `maxTransfers` callbacks. A normal
  shared-bus owner should use `maxTransfers = 1`.
- `cancelJob()` performs no I2C. It discards partial sample work and publishes a
  terminal cancellation result. `takeJobResult()` consumes that result exactly
  once; a pending result blocks admission of another job.

Request IDs must be nonzero. Reuse an ID only after its previous terminal
result has been taken. A start call returning `IN_PROGRESS` means admission,
not completion.

### Minimal owner loop

The transport functions below are application adapters; see the Arduino and
native ESP-IDF examples for complete single-attempt implementations.

```cpp
#include "INA3221/INA3221.h"

INA3221::INA3221 monitor;
uint32_t nextRequestId = 1;

INA3221::TransportConfig transportConfig() {
  INA3221::TransportConfig t{};
  t.i2cWrite = appI2cWrite;          // exactly one bounded attempt
  t.i2cWriteRead = appI2cWriteRead;  // exactly one bounded attempt
  t.i2cUser = &applicationBus;
  t.defaultTransferTimeoutMs = 20;
  t.offlineThreshold = 5;            // passive telemetry only
  return t;
}

INA3221::DeviceProfile deviceProfile() {
  INA3221::DeviceProfile p{};
  p.i2cAddress = 0x40;
  p.enabledChannels = INA3221::ALL_CHANNELS;
  p.mode = INA3221::Mode::SHUNT_BUS_TRIG;
  for (auto& shunt : p.shunts) {
    shunt.resistanceMicroOhms = 100000;  // 100 milliohms
  }
  return p;
}

void start(uint64_t nowMs) {
  auto status = monitor.bind(transportConfig(), deviceProfile()); // no I2C
  if (!status.ok()) return;
  status = monitor.startInitialize(nextRequestId++, nowMs + 1000);
  // Require status.inProgress(); completion arrives through JobResult.
}

void service(uint64_t nowMs, uint64_t ownerDeadlineMs) {
  INA3221::PollContext poll{};
  poll.nowMs = nowMs;
  poll.deadlineMs = ownerDeadlineMs; // effective deadline is the earlier limit
  poll.transferTimeoutMs = 20;
  poll.maxTransfers = 1;
  (void)monitor.pollJob(poll);

  INA3221::JobProgress progress{};
  (void)monitor.getJobProgress(progress); // cache-only
  if (!progress.resultPending) return;

  INA3221::JobResult result{};
  if (!monitor.takeJobResult(result).ok()) return; // exactly once
  if (!result.status.ok()) {
    // Inspect result.state and result.hardwareEffect before retry/reconcile.
    return;
  }
  if (result.sampleValid) {
    consume(result.sample); // fixed-size, fixed-unit, provenance-bearing batch
  }
}
```

After successful initialization, start an atomic triggered acquisition with
`startTriggeredSample(SHUNT_BUS_TRIG, id, deadline)`, or a mixed-age continuous
read with `startContinuousSample(id, deadline, consumeAlertSnapshot)`.

## Result, certainty, and alert contracts

`SampleBatch` contains exactly three fixed slots, enabled/valid channel masks,
per-quantity validity, integer microvolts/millivolts/milliamps/milliwatts, an
alert snapshot, coherence (`TRIGGERED_ATOMIC` or `CONTINUOUS_MIXED_AGE`), capture
uptime, profile generation, and request ID. Partial work is never exposed as a
last-good sample. `peekLastSample()` is cache-only.

Every terminal `JobResult` reports its job kind, request ID, terminal state,
transport count, `Status`, profile generation, and hardware effect
(`NONE`, `CONFIRMED`, `PARTIAL`, or `INDETERMINATE`). Ambiguous write failures
are not reported as clean rollback. Inspect `measurementConfigState()` and
`alertConfigState()`:

- `APPLIED`: the managed profile was read back and verified.
- `DIRTY`: a confirmed side effect changed a managed register and reconciliation
  is required.
- `UNKNOWN`: hardware acceptance is ambiguous. Do not admit measurement work;
  run `startReconcile()` or a full `startInitialize()` under owner policy.

Mask/Enable reads are destructive for CVRF and latched alerts. Every library
read of that register first retains destructive event bits. `peekAlertEvents()`
is non-consuming; `takeAlertEvents()` clears only the retained event bits after
copying them. The current raw value, writable bits, PVF, TCF, and CVRF remain
explicit in `AlertSnapshot`.

## Deterministic bounds

`maximumJobTransfers()` returns the successful-path ceiling for a validated
profile when triggered CVRF is high on its first eligible check. With all three
channels enabled:

| Job | Maximum callbacks |
|---|---:|
| `INITIALIZE` | 35 |
| `APPLY_PROFILE` | 33 |
| `RECONCILE` | 33 |
| `TRIGGERED_SAMPLE` | 8 |
| `CONTINUOUS_SAMPLE` | 7 |
| `POWER_DOWN` | 3 |

For `N` enabled channels, triggered sampling is bounded by `2 + 2N` and
continuous sampling by `1 + 2N`. Profile jobs are conservative read, optional
write, and readback-verify bounds; already-matching registers use fewer calls.
Power-down is read, optional write, and verify.

If triggered CVRF is low at the first eligible check, the engine retains the
observed alert bits, waits for a strictly later caller timestamp, then arms an
additional 1 ms wait before reading Mask/Enable again. Therefore the fault-path
read count is bounded by the number of eligible owner polls before the absolute
deadline, not by the 8-callback success figure. Every owner job requires a
finite absolute deadline; a poll deadline may only shorten it. Poll cadence and
the deadline are application policy and make the retry ceiling calculable for
that owner.

At `maxTransfers = 1`, one `pollJob()` call performs at most one synchronous
callback, so its transport blocking contribution is at most the smaller of the
poll timeout, configured default timeout, and remaining effective deadline,
plus bounded local CPU work. For a larger budget, the driver divides the
remaining effective deadline by `maxTransfers` and caps every admitted callback
to that per-transfer share. The sum of callback timeout bounds in one poll
therefore cannot exceed the remaining deadline. If the share rounds to zero,
the poll performs no I2C and terminalizes as expired. Deadline expiration is
observable and produces a take-once terminal result.

### Conversion timing

`conversionTiming()` exposes datasheet typical and library scheduling maximum
values per conversion:

| Setting | Typical (us) | Maximum (us) |
|---|---:|---:|
| `CT_140US` | 140 | 154 |
| `CT_204US` | 204 | 224 |
| `CT_332US` | 332 | 365 |
| `CT_588US` | 588 | 646 |
| `CT_1100US` | 1,100 | 1,210 |
| `CT_2116US` | 2,116 | 2,328 |
| `CT_4156US` | 4,156 | 4,572 |
| `CT_8244US` | 8,244 | 9,068 |

`maximumCycleTimeUs()` multiplies the relevant maximum shunt/bus times by the
enabled-channel count and averaging sample count. After the trigger callback
returns, a triggered job requires a subsequent poll with a strictly later
timestamp; that timestamp becomes the conversion-wait origin. The job then
waits the maximum cycle plus a fixed 100 us wake margin, rounded up to
milliseconds, before the first CVRF/alert read. This avoids treating the time
captured before a synchronous callback as the conversion start. Before writing
the trigger, the driver bus-silently rejects a deadline that cannot contain the
one-tick origin advance, maximum conversion wait, and successful-path callback
bounds. For the largest three-channel, shunt+bus, 1024-sample profile, the
typical cycle is 50,651,136 us and the scheduling maximum is 55,713,792 us
before the 100 us margin; a 1000 ms owner deadline is therefore rejected before
I2C.

Rare-operation latency is not applicable: INA3221 has no EEPROM/NVM and all
registers are volatile. There are no hidden erase, program, or persistence
waits.

## Ownership and concurrency

- One application context owns the object and its bus admission. Serialize all
  methods and all callbacks in that same context.
- The class is non-copyable and non-movable, non-reentrant, and not ISR-safe.
- Do not call any driver method from a transport callback.
- The driver owns no I2C instance, task, mutex, queue, retry/recovery policy,
  scheduling policy, or deadline renewal.
- No public method is safe to race with `pollJob()`, `cancelJob()`, `unbind()`,
  or result/cache access. External serialization is mandatory.
- Health counters and `READY`/`DEGRADED`/`OFFLINE` are passive diagnostics.
  `OFFLINE` never gates or silently suppresses an admitted transfer. The bus
  owner decides admission, backoff, retry, and recovery.
- Failure/success totals are object-lifetime diagnostics and intentionally
  survive `bind()`/`unbind()`. A successful initialization always restores the
  `READY`/zero-consecutive-failure invariant; initialization transfers are not
  counted as initialized steady-state successes.
- Core library paths do not log, call Arduino/ESP-IDF APIs, or allocate heap in
  steady operation.

## Legacy synchronous compatibility

`Config`, `begin()`, direct getters/setters, raw register access,
`readBlocking()`, `probe()`, and `recover()` remain bounded compatibility tools
for standalone bring-up and diagnostics. They are not the recommended shared
I2C-owner steady path.

| Compatibility operation | Worst-case transfer behavior |
|---|---|
| `begin()` / `recover()` | Up to 35 synchronous callbacks in one call |
| `probe()` | Two synchronous identity reads; intentionally does not update health |
| Direct raw/scaled read or direct setter | Normally one synchronous callback; `readChannel()` and `readPower()` use two |
| `powerDown()` | Up to three synchronous callbacks (read/write/verify) |
| `readBlocking()` | Internally polls with budget one; up to 8 callbacks for three-channel triggered sampling or 7 for continuous sampling, plus bounded cooperative polls until its timeout |
| Legacy staged APIs | Caller-supplied instruction budget; wrappers share the single owner engine and arm a derived finite deadline on first poll |

Each callback still has the configured transfer timeout. A multi-transfer
synchronous call can therefore block for the sum of its callback bounds plus
local work. Compatibility calls reject an active owner job; new jobs reject a
legacy conversion in progress. Direct diagnostic writes can make profile
certainty `DIRTY` or `UNKNOWN`. Do not mix compatibility mutation with the
owner-safe engine without explicit serialization and reconciliation.
The staged compatibility poll methods extend their monotonic 32-bit time input
through wrap for the active job; callers must poll at least once per 32-bit
clock period. `readBlocking()` uses wrap-safe unsigned elapsed time. Production
code should still use the 64-bit `PollContext` owner API and its explicit
deadline.

## Installation and examples

PlatformIO:

```ini
lib_deps =
  https://github.com/janhavelka/INA3221.git#v3.0.0
```

For ESP-IDF, add this repository as a component through
`EXTRA_COMPONENT_DIRS` or component-manager metadata. The core has no framework
headers. See:

- `examples/01_basic_bringup_cli/` for Arduino/PlatformIO. Startup uses
  zero-I2C bind, budget-one initialization, a triggered sample, progress, and
  take-once result handling. `job sample`, `job cancel`, and `job` expose the
  cooperative flow while the existing diagnostic CLI remains available.
- `examples/esp_idf/basic/` for native `app_main`,
  `driver/i2c_master.h`, `esp_timer`, FreeRTOS waits, and fixed C buffers. It
  uses the same owner-safe flow without Arduino facades.
- `examples/common/` is Arduino example glue, not library code.

## Validation

```bash
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python tools/check_core_timing_guard.py
python tools/check_metadata_consistency.py
python scripts/generate_version.py check
python tools/check_strict_compile.py
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

CI compiles the native tests, both Arduino targets, and both native ESP-IDF
targets. A local static contract pass is not an ESP-IDF compile; when `idf.py`
is unavailable, report that build as not run rather than inferring success.

## Documentation

- `CHANGELOG.md` - release history
- `docs/IDF_PORT.md` - native ESP-IDF integration and ownership contract
- `docs/IDF_PORT_IMPLEMENTATION.md` - implemented files and validation scope
- `docs/TUNNELMONITOR_NODE_SUITABILITY_AUDIT.md` - host integration audit

## License

MIT License. See `LICENSE`.
