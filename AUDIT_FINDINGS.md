# INA3221 library audit — findings and proposed fixes

Audit date: 2026-08-27 · Library version audited: `3.1.0` (`9c18102`)

Reference: bundled TI datasheet SBOS576C (May 2012, revised September 2025),
`docs/INA3221_datasheet.pdf`.

This document is a working list, not maintained documentation. Delete it once
the items are resolved and the behaviour changes are recorded in
[CHANGELOG.md](CHANGELOG.md).

---

## 1. Scope and method

Every register address, bit mask, reset value, LSB, conversion-time table entry
and encode/decode routine was checked against the datasheet register map
(§7.6) and electrical table (§7.5). The cooperative job engine, the legacy
compatibility facade, the alert/Mask-Enable paths, both examples, the Python
tooling and the build metadata were reviewed for functional defects.

Behaviour claims below were reproduced on the host against a modelled INA3221
(power-on-reset register defaults, read-clear Mask/Enable) unless marked
"by inspection". Candidate findings were then re-checked adversarially against
the code, the datasheet, the test suite and the existing contracts; §4 lists
what did *not* survive that check, so the next audit does not re-open it.

Baseline before the audit: 124/124 native tests pass; all static checkers pass;
Doxygen builds warning-clean.

**The datasheet-facing layer is correct.** Register addresses, POR values,
Configuration and Mask/Enable bit positions, the 13-bit `[15:3]` shunt/bus
format with 40 µV / 8 mV LSBs, the 15-bit `[15:1]` shunt-sum format, the
`0xFFF8` / `0xFFFE` / `0x7FF8` writable masks (bit 15 of the power-valid limits
is read-only per Tables 7-38/7-40), the typical and maximum conversion-time
tables (140 µs…8.244 ms / 154 µs…9068 µs), the averaging table and the profile
defaults all match the datasheet exactly. No decoding or scaling bug was found.

The defects are in the **driver's own state machine and lifecycle**.

---

## 2. Already fixed in this pass

Applied directly because they are small and unambiguous:

| Change | File |
|---|---|
| `PollJobSnapshot::nextChannel` / `conversionStartMs` are now populated from the live job cursor instead of being hard-wired to zero | `src/INA3221.cpp` |
| `readConversionReady()` Doxygen no longer promises a `CONVERSION_NOT_READY` return it never produces | `include/INA3221/INA3221.h` |
| `end()` Doxygen no longer claims a best-effort power-down; it is bus-silent | `include/INA3221/INA3221.h` |
| `softReset()` Doxygen states the actual certainty outcome (DIRTY on a confirmed reset, UNKNOWN on an ambiguous one) | `include/INA3221/INA3221.h` |
| `writeRegister16()` Doxygen states that *every* accepted or ambiguous raw write invalidates certainty, not only Configuration/Mask-Enable | `include/INA3221/INA3221.h` |
| `profileGeneration()` Doxygen mentions the `setShuntResistance()` bump | `include/INA3221/INA3221.h` |
| `SampleBatch::validChannels` Doxygen matches the derived-quantity gating actually implemented | `include/INA3221/INA3221.h` |
| `AGENTS.md`: replaced the removed v2 "Managed Synchronous Driver" architecture section, the v2 `begin/tick/end` lifecycle rule, the stale `recover()`-uses-`probe()` claim, the "counters wrap at max" claim (they saturate), and the repository tree that omitted `test/`, `tools/`, `scripts/`, `docs/`, CI and build metadata | `AGENTS.md` |

Everything else needs a decision and is listed below.

---

## 3. Findings

Severity: **critical** = a documented workflow does not work on real hardware;
**high** = wrong results or an unrecoverable state; **medium** = wrong
diagnostics or a silently wrong side effect; **low** = dead code, contract drift.

### C1 — `readBlocking()` bounds its poll loop by iteration count, not time

`src/INA3221.cpp:2292` (`readBlocking`)

```cpp
const uint64_t maxPolls = static_cast<uint64_t>(timeoutMs) +
                          static_cast<uint64_t>(maximumTransfers) * 3U + 4U;
for (uint64_t polls = 0; polls < maxPolls && !_hasPendingJobResult; ++polls) { ... }
if (!_hasPendingJobResult) (void)cancelJob();
```

The loop spins as fast as the CPU allows; nothing ties one iteration to one
millisecond. A triggered sample must wait for the maximum conversion cycle
(8 ms for the default three-channel shunt+bus/AVG_1 profile, 30 ms at AVG_4,
up to ~52 s at AVG_1024). The budget is ≈ `timeoutMs + 28` iterations, so on any
host that completes more than ~150 iterations per millisecond the budget is
exhausted long before the conversion finishes and the call returns `CANCELLED`.

Reproduced on the host with the loop rate as the only variable:

| loop iterations per simulated ms | result |
|---|---|
| 1 | `OK` (elapsed 40 ms) |
| 50 | `OK` (elapsed 9 ms) |
| 1 000 | `CANCELLED` (elapsed 1 ms) |
| 20 000 (ESP32-class) | `CANCELLED` (elapsed 0 ms) |

The native tests pass because `FakeBus::advanceOnYieldMs = 1` advances the fake
clock a full millisecond per iteration. The HIL suite passes because every
`read` step runs with the profile in continuous mode (`DATA-003A "mode sbc"`
immediately precedes the direct-read block), where no conversion wait exists.

Continuous mode is unaffected. Triggered mode is broken on hardware, and
`readBlocking()` is what the `read` command in both example CLIs calls.

**Proposed fix.** `pollJob()` already enforces the deadline itself
(`context.nowMs >= deadlineMs` → `DEADLINE_EXPIRED`), so the loop only needs a
guard against a monotonic clock that never advances. Replace the iteration
budget with a time bound plus a stall guard:

```cpp
uint32_t lastElapsed = 0;
uint32_t stalledSpins = 0;
while (!_hasPendingJobResult) {
  const uint32_t elapsed = static_cast<uint32_t>(_nowMs() - startMs);
  if (elapsed >= timeoutMs) break;              // caller's own bound
  if (elapsed != lastElapsed) { lastElapsed = elapsed; stalledSpins = 0; }

  PollContext context{};
  context.nowMs = elapsed;
  context.deadlineMs = deadlineMs;
  context.transferTimeoutMs = _transport.defaultTransferTimeoutMs;
  context.maxTransfers = 1U;
  const uint16_t before = _jobTransfers;
  st = pollJob(context);
  if (!st.ok() && !st.inProgress() && !_hasPendingJobResult) return st;
  if (_jobTransfers == before && ++stalledSpins > STALLED_CLOCK_SPIN_LIMIT) break;
  if (!_hasPendingJobResult) _cooperativeYield();
}
```

with `static constexpr uint32_t STALLED_CLOCK_SPIN_LIMIT = 1000000;` — reset on
every observed time change or transfer, so it can only fire when the clock is
genuinely stuck. `test_read_blocking_times_out_with_stalled_clock` keeps
passing (it uses a clock that never advances and expects a non-OK result);
add a test that drives many iterations per simulated millisecond.

---

### C2 — Every typed configuration setter blocks all measurement reads until `recover()`

`src/INA3221.cpp` — `setMode`, `setAveraging`, `setVBusConvTime`,
`setVShuntConvTime`, `setChannelEnable`, `startConversion`, `startConversion(Mode)`

Each of those performs a successful, fully-specified Configuration-register
write and then unconditionally calls `_markRegisterDirty(cmd::REG_CONFIG)`,
which drops `measurementConfigState()` from `APPLIED` to `DIRTY`.
`_ensureMeasurementReadyForRead()` requires `APPLIED`, so every direct read
afterwards fails with `CONFIG_UNKNOWN`:

```
begin                       -> OK
  meas=APPLIED alert=APPLIED dirty=0
read right after begin      -> OK
setAveraging(AVG_16)        -> OK
  meas=DIRTY dirty=1 reason='Hardware profile requires verification'
read after setAveraging     -> CONFIG_UNKNOWN 'Measurement profile not verified'
readChannel                 -> CONFIG_UNKNOWN
readBlocking                -> CONFIG_UNKNOWN
startContinuousRead         -> CONFIG_UNKNOWN
```

`startConversion()` has the same effect, so the documented legacy single-shot
flow — trigger, wait for CVRF, read the channel registers — can never complete.

The only escape is `recover()`, a 35-callback full re-initialisation. This is
exactly the "bench-test one device" workflow, and the HIL suite has the
workaround baked in — `BASE-001..007` set mode/averaging/CT/channels and
`BASE-008` is a bare `recover`.

The driver *knows* what it wrote. Marking the register unverified after a
confirmed write of the complete desired value is unnecessarily pessimistic.

**Proposed fix.** Give the setters the same write-then-verify discipline the
job engine already uses, in one shared helper, and delete the seven copies of
the save/restore/`_markRegisterDirty` boilerplate:

```cpp
// Write the complete desired Configuration register and verify it by readback.
// One extra transfer buys the same certainty the cooperative engine provides.
Status INA3221::_applyConfigVerified() {
  const uint16_t desired = _buildConfigRegister();
  Status st = _writeRegister16Tracked(cmd::REG_CONFIG, desired);
  if (!st.ok()) {
    if (_writeMayHaveReachedDevice(st)) _markRegisterUnknown(cmd::REG_CONFIG);
    return st;
  }
  _markRegisterDirty(cmd::REG_CONFIG);
  uint16_t actual = 0;
  st = _readRegister16Tracked(cmd::REG_CONFIG, actual);
  if (!st.ok()) return st;                                  // stays DIRTY
  if (!_registerMatches(cmd::REG_CONFIG, actual, desired)) {
    return Status::Error(Err::PROFILE_MISMATCH,
                         "Configuration verification mismatch", cmd::REG_CONFIG);
  }
  _measurementConfigState = AppliedConfigState::APPLIED;
  if (_alertConfigState == AppliedConfigState::APPLIED) _clearHardwareConfigDirty();
  return Status::Ok();
}
```

Reading the Configuration register is explicitly side-effect-free
(datasheet §7.6.2.1: "This register can be read from at any time without
impacting or affecting either device settings or conversions in progress"), so
the readback is safe even immediately after a trigger write.

Each setter then becomes: validate → stage the candidate state →
`_applyConfigVerified()` → restore on failure. Update the README compatibility
table: setters become two callbacks.

---

### C3 — An outstanding legacy conversion permanently locks out the whole API

`src/INA3221.cpp:672` (`_startJob`), `:545` (`bind`)

`startConversion()` sets `_conversionStarted`, which is only cleared by
observing CVRF, by another Configuration write, by a reset, or by
`unbind()`/`end()`. If CVRF is never observed — a device fault, a lost
destructive read, or another bus master consuming the flag — the object is
dead:

```
startConversion            -> IN_PROGRESS 'Conversion started'
readConversionReady        -> OK, ready=0        (never becomes ready)
startInitialize            -> CONVERSION_BUSY 'Legacy conversion active'
startPowerDown             -> CONVERSION_BUSY
cancelJob                  -> NO_ACTIVE_JOB      (nothing to cancel)
bind (rebind attempt)      -> CONVERSION_BUSY
recover                    -> CONVERSION_BUSY
startInitialize after      -> CONVERSION_BUSY
```

There is no `cancelConversion()`. `recover()`, the documented manual recovery
path, cannot recover. `bind()` refuses even though its first action is
`unbind()`, which would have cleared the flag — the guard protects nothing and
only blocks the escape route.

No test asserts `CONVERSION_BUSY`, so nothing depends on the current behaviour.

**Proposed fix**, in three small parts:

1. `bind()`: drop the `_conversionStarted` guard. It calls `unbind()`
   unconditionally and its Doxygen already says a successful call discards
   prior state.
2. `_startJob()`: replace the `CONVERSION_BUSY` rejection with a bus-silent
   abandonment of the stale trigger for the job kinds that rewrite the
   Configuration register anyway (`INITIALIZE`, `APPLY_PROFILE`, `RECONCILE`,
   `POWER_DOWN`). Keep the rejection for `TRIGGERED_SAMPLE` /
   `CONTINUOUS_SAMPLE`, where silently mixing the two APIs would produce a
   sample of unclear provenance.
3. Add a bus-silent `Status cancelConversion();` to the compatibility API that
   clears `_conversionStarted` / `_conversionReady` / `_conversionStartMs` and
   returns `NO_ACTIVE_JOB` when nothing was outstanding. Expose it in both
   example CLIs.

---

### H1 — The legacy float current/power path ignores `ShuntCalibration::direction`

`src/INA3221.cpp:1925` (`readCurrent`), `:1975` (`readChannel`), and
`readPower()` which derives from `readCurrent()`

```cpp
float rShunt = _config.shuntResistance[static_cast<uint8_t>(ch)];
mA = shuntMv / rShunt;                      // no direction applied
```

`calculateCurrentMilliAmps()` (`src/INA3221.cpp:412`) — used by every fixed-unit
path, including `SampleBatch` — honours
`CurrentDirection::POSITIVE_SHUNT_IS_NEGATIVE_CURRENT` and negates. The float
path does not. The root cause is `_syncLegacyConfigFromProfile()`
(`src/INA3221.cpp:593`), which copies only `resistanceMicroOhms` into
`_config.shuntResistance[]`; `Config` has no direction field at all
(`include/INA3221/Config.h:196`), and `_legacyToContracts()` never sets
`profile.shunts[i].direction` either.

For a channel bound with the inverted convention and a shunt register of
`0x2710` (+50 000 µV over 0.1 Ω):

- owner path (`startContinuousSample` → `convertRawChannel`): `-500 mA`
- `readCurrent()` on the same object: `+500.0f`

`readPower()` and `readChannel()` inherit the wrong sign. Both paths are live
on the same instance after `bind()` + `startInitialize()`, and the bring-up CLI
exposes both (`direction <ch> <0|1>` next to `channel`). Nothing in the Doxygen
warns about it, and `CurrentDirection` is documented in `Config.h:110` as
"Sign convention applied after the shunt-voltage measurement" — a blanket claim
the legacy path violates.

**Proposed fix.** Route the float helpers through the same fixed-unit
calculation instead of re-deriving the maths:

```cpp
Status INA3221::readCurrent(Channel ch, float& mA) {
  Status valid = validateMeasurementRead(_config, _initialized, ch, true, false);
  if (!valid.ok()) return valid;
  int16_t raw = 0;
  Status st = readShuntRaw(ch, raw);
  if (!st.ok()) return st;
  int32_t microVolts = 0;
  (void)decodeShuntMicroVolts(static_cast<uint16_t>(raw), microVolts);
  int32_t milliAmps = 0;
  st = calculateCurrentMilliAmps(microVolts,
                                 _profile.shunts[static_cast<uint8_t>(ch)],
                                 milliAmps);
  if (!st.ok()) return st;
  mA = static_cast<float>(milliAmps);
  return Status::Ok();
}
```

Do the same in `readPower()` and `readChannel()`. This removes the duplicated
`shuntMv / rShunt` and `busV * currentMa` expressions and makes the two APIs
agree by construction. Add `CurrentDirection direction[3]` to `Config` (or an
overload `setShuntResistance(Channel, float, CurrentDirection)`) so the
convention is reachable from the legacy path at all.

The fixed-unit path rounds to whole milliamps, so this changes the float
result's resolution. If sub-milliamp resolution matters for bench work, keep
the float division and apply only the sign:

```cpp
mA = shuntMv / rShunt;
if (_profile.shunts[static_cast<uint8_t>(ch)].direction ==
    CurrentDirection::POSITIVE_SHUNT_IS_NEGATIVE_CURRENT) {
  mA = -mA;
}
```

Pick one; do not leave the two paths disagreeing.

---

### H2 — The timing-control alert can never be reported

`src/INA3221.cpp:868` (`_retainMaskEnable`), `include/INA3221/INA3221.h`
(`AlertSnapshot::timingControl`, `AlertFlags::timingControl`)

```cpp
current.timingControl = (raw & cmd::MASK_TCF) != 0U;
_retainedAlerts.timingControl = _retainedAlerts.timingControl || current.timingControl;
```

Per the datasheet the TC pin is open-drain and **pulls low to signal the fault**
(§7.3.2.4: "the timing control (TC) alert pin pulls low to indicate that the
INA3221 has not detected a valid power rail on channel 2"). TCF "corresponds to
the status of the TC pin" and its power-on reset value is **1**
(Table 7-36; Mask/Enable POR = `0x0002`).

So `TCF == 1` means *no fault*, and the library latches exactly the wrong edge:
the sticky OR makes `timingControl` permanently `true` from the first read after
power-up, and the transition to `0` — the actual alert — is discarded. A field
named `timingControl` inside `AlertSnapshot`/`AlertFlags`, next to
`criticalCh1`/`warningCh1`, reads as "alert asserted"; it is the opposite.

**Proposed fix.** Keep the raw bit but stop treating it as an event, and latch
the real one:

```cpp
// INA3221.h
struct AlertSnapshot {
  ...
  bool timingControl = false;      ///< Latest TCF bit: true = TC pin high = no
                                   ///< timing-control fault. POR value is true.
  bool timingControlFault = false; ///< Sticky: TCF was observed low (TC asserted)
  ...
};

// _retainMaskEnable
current.timingControl = (raw & cmd::MASK_TCF) != 0U;
_retainedAlerts.timingControl = current.timingControl;             // level, not sticky
_retainedAlerts.timingControlFault =
    _retainedAlerts.timingControlFault || !current.timingControl;  // latch the fault
```

Same change for the `consumed` snapshot; clear `timingControlFault` in
`takeAlertEvents()` alongside `events`. `AlertSnapshot` stays trivially
copyable, so the `static_assert` on `SampleBatch` holds. Update the Doxygen on
both `timingControl` fields to state the polarity, and print both in the two
example CLIs.

Note also that the datasheet says the timing-control function is only armed at
power-up or after a software reset, and that writing the Configuration register
before the sequence completes disables it until the next power cycle or reset
(§7.3.2.4). `startInitialize()` writes the Configuration register, so in
practice this library disables the TC function on every bring-up. Document that
in the README rather than trying to work around it.

---

### H3 — Transfer outcomes are classified by status code instead of by whether a callback ran

`src/INA3221.cpp:918` (`_ambiguousWriteFailure`), `:3020`/`:3038`
(`_i2cWriteReadTracked` / `_i2cWriteTracked`), `:906`/`:914`
(`_jobReadRegister` / `_jobWriteRegister`)

Three decisions all sniff the returned `Err` value:

```cpp
bool INA3221::_ambiguousWriteFailure(const Status& status) {
  return status.code != Err::I2C_NACK_ADDR && status.code != Err::DEVICE_NOT_FOUND &&
         status.code != Err::INVALID_CONFIG && status.code != Err::INVALID_PARAM;
}

Status INA3221::_i2cWriteTracked(...) {
  Status st = _i2cWriteRaw(buf, len, timeoutMs);
  if (st.code == Err::INVALID_CONFIG || st.code == Err::INVALID_PARAM) return st;
  return _updateHealth(st);
}
```

The intent is "did anything reach the bus?", but the proxy is wrong in both
directions.

*A real bus failure is silently not counted.* The bundled Arduino adapter maps
`Wire::endTransmission() == 1` to `INVALID_PARAM` ("I2C data too long"), and the
ESP-IDF adapter returns `INVALID_PARAM` for an out-of-range timeout. Injecting
each code into a tracked read:

| adapter returns | driver state after | consecutiveFailures | totalFailures | lastError |
|---|---|---|---|---|
| `I2C_BUS` | DEGRADED | 1 | 1 | `I2C_BUS` |
| `I2C_NACK_ADDR` | DEGRADED | 1 | 1 | `I2C_NACK_ADDR` |
| `I2C_TIMEOUT` | DEGRADED | 1 | 1 | `I2C_TIMEOUT` |
| **`INVALID_PARAM`** | **READY** | **0** | **0** | **OK** |
| **`INVALID_CONFIG`** | **READY** | **0** | **0** | **OK** |

and on the write path the same two codes report `hardwareConfigDirty = false`,
i.e. the driver asserts the write definitely did not reach the device.

*A transfer that never happened is reported as an ambiguous write.*
`_jobWriteRegister()` returns `DEADLINE_EXPIRED` **before** calling the
transport when the clamped timeout rounds to zero. `_pollProfileJob`'s
`PROFILE_WRITE` branch then runs `_ambiguousWriteFailure(DEADLINE_EXPIRED)` →
`true` → `_markRegisterUnknown()` and `JobTerminalState::INDETERMINATE` with
`HardwareEffect::INDETERMINATE`, for a purely bus-silent budget rejection. The
owner is told the device may be in an unknown state when nothing was
transmitted, and the README explicitly tells them not to retry such a write.

**Proposed fix.** Decide by construction, not by inspection. The raw wrappers
already know whether they invoked the callback — make them say so:

```cpp
// Private state, set by _i2cWriteRaw/_i2cWriteReadRaw immediately before the call.
bool _lastTransferAttempted = false;

Status INA3221::_i2cWriteRaw(const uint8_t* buf, size_t len, uint32_t timeoutMs) {
  _lastTransferAttempted = false;
  if (_config.i2cWrite == nullptr) return Status::Error(Err::INVALID_CONFIG, ...);
  if (buf == nullptr || len == 0)  return Status::Error(Err::INVALID_PARAM, ...);
  _lastTransferAttempted = true;
  return _config.i2cWrite(_config.i2cAddress, buf, len, timeoutMs, _config.i2cUser);
}

Status INA3221::_i2cWriteTracked(const uint8_t* buf, size_t len, uint32_t timeoutMs) {
  const Status st = _i2cWriteRaw(buf, len, timeoutMs);
  return _lastTransferAttempted ? _updateHealth(st) : st;
}

bool INA3221::_writeMayHaveReachedDevice(const Status& status) const {
  if (!_lastTransferAttempted) return false;        // bus-silent: definitely not
  return status.code != Err::I2C_NACK_ADDR &&
         status.code != Err::DEVICE_NOT_FOUND;      // address phase never ACKed
}
```

`_jobWriteRegister()` must also clear `_lastTransferAttempted` on its
budget/deadline early-returns. Replace every `_ambiguousWriteFailure(st)` call
with `_writeMayHaveReachedDevice(st)`.

This fixes all three symptoms at once, and adapters no longer have to know
which `Err` values the driver treats specially. State the new rule in the
README transport section: *"the driver classifies a failed write by whether it
invoked your callback, not by the code you return; return the most accurate
code you have."*

---

### H4 — `readShuntSumRaw()` / `readShuntSumVoltage()` return stale data as valid

`src/INA3221.cpp:1972`

Both check only `_initialized` and `_ensureMeasurementReadyForRead()`. The
Shunt-Voltage Sum register is filled from *single shunt-voltage conversions of
the channels selected by SCC1-3* and is updated "following each complete cycle
of all selected channels" (datasheet §7.6.2.14). In `BUS_TRIG`/`BUS_CONT` no
shunt conversion runs, and with `summationChannels == 0` no channel feeds the
register — in both cases the read returns a stale or zero value with `OK`.

Every other measurement read is gated by `validateMeasurementRead()`, which
rejects a mode that does not measure the requested quantity. The sum read is
the one that was left out.

**Proposed fix.** Add the two missing preconditions:

```cpp
if (!modeReadsShunt(_profile.mode)) {
  return Status::Error(Err::INVALID_CONFIG, "Mode does not measure shunt");
}
if (_profile.alerts.summationChannels == 0U) {
  return Status::Error(Err::INVALID_CONFIG, "No summation channel selected");
}
```

Also worth rejecting in `_validateProfile()`: a non-zero `summationChannels`
with a bus-only or power-down mode is a configuration that can never produce a
sum.

---

### H5 — The Arduino example transport rejects any callback deadline tighter than the Wire timeout

`examples/common/I2cTransport.h:76`

```cpp
if (timeoutMs < context->configuredTimeoutMs) {
  return INA3221::Status::Error(INA3221::Err::I2C_TIMEOUT,
                                "Requested timeout is below Wire timeout");
}
```

`_clampedTransferTimeout()` divides the remaining deadline by
`PollContext::maxTransfers`, so any budget larger than
`remaining / configuredTimeoutMs` produces a tighter per-callback bound. The
CLI advertises `job step <0..255>` with a 5 000 ms job window: at
`maxTransfers = 255` the share is 19 ms against a 50 ms Wire timeout, so
**every** callback is refused. The same happens in the last 50 ms before any
deadline at `maxTransfers = 1`.

The refusal is bus-silent, but it is reported as `I2C_TIMEOUT`, which today
(see H3) counts as a transport-health failure and marks the target register
`UNKNOWN` / the job `INDETERMINATE`.

The comment explaining the refusal is sound — `TwoWire` has a bus-level
timeout, and reconfiguring a shared bus inside a callback would be worse.

**Proposed fix**, in this order:

1. Apply H3. With `_lastTransferAttempted` the adapter's pre-flight rejection is
   correctly classified as bus-silent because it never calls `Wire`.
2. Have the CLI stop advertising budgets the transport cannot honour: clamp
   `job step <n>` so that `OWNER_JOB_TIMEOUT_MS / n >= board::I2C_TIMEOUT_MS`,
   and print why when it clamps.
3. Document in `docs/IDF_PORT.md` and the Arduino example README that an
   adapter whose backend only supports a bus-level timeout must be given
   `maxTransfers` values consistent with it.

---

### M1 — `_config` and `_profile` are two sources of truth for the same device state

`include/INA3221/INA3221.h:1100` (`Config _config;` beside `DeviceProfile _profile;`)

Every field of `_config` is already present in `_transport` + `_profile`:
callbacks, context, address, timeout and offline threshold in `TransportConfig`;
channel enables, averaging, conversion times, mode and shunt resistance in
`DeviceProfile`. `_config` is written by 88 sites in `src/INA3221.cpp` and read
by `_isTriggeredMode()`, `_isContinuousMode()`, `_enabledChannelCount()`,
`_buildConfigRegister()`, `getCycleTimeUs()`, `getConversionTimeUs()` and
`validateMeasurementRead()`, while the job engine and `_readConversionReadyAt()`
read `_profile`. They diverge:

```
begin(SHUNT_BUS_CONT)                -> in sync
writeConfig(0x4FFB)                  -> config.mode=SHUNT_BUS_TRIG  profile.mode=SHUNT_BUS_CONT
                                        config.avg=AVG_1024         profile.avg=AVG_1
softReset() from SHUNT_TRIG/AVG_64   -> config.mode=SHUNT_BUS_CONT  profile.mode=SHUNT_TRIG
                                        config.avg=AVG_1            profile.avg=AVG_64
```

`_readConversionReadyAt()` is the sharpest edge: it decides *whether* to gate on
a conversion delay from `_config.mode` (`_isTriggeredMode()`) but computes *how
long* that delay is from `maximumCycleTimeUs(_profile, _profile.mode, …)`. After
`writeConfig()` the two describe different devices, so the readiness gate opens
after 8 ms for a conversion that actually needs 1.24 s and burns destructive
Mask/Enable reads polling for a CVRF that cannot be set yet. H1's dropped
`direction` field is the same root cause seen from the other side.

**Proposed fix.** Make `_profile` the single source of truth and stop storing
`_config` at all:

1. Delete the `Config _config;` member. Keep the `Config` *type* — it is the
   public compatibility input.
2. Add `Config INA3221::getConfig() const;` (by value) that materialises the
   struct from `_transport` + `_profile`. All twelve existing test uses
   (`dev.getConfig().i2cAddress` etc.) bind to a temporary and keep compiling.
3. Replace `_config.i2cWrite` / `i2cAddress` / `i2cTimeoutMs` / `i2cUser` /
   `nowMs` / `timeUser` / `cooperativeYield` / `offlineThreshold` in the
   transport wrappers and health code with the `_transport` fields they were
   copied from.
4. Replace `_config.mode` / `averaging` / `vBusCt` / `vShCt` / `chNEnable` /
   `shuntResistance[]` with the `_profile` equivalents;
   `validateMeasurementRead()` takes a `const DeviceProfile&`.
5. Delete `_syncLegacyConfigFromProfile()` and the `Config prevConfig = _config;`
   save/restore blocks in the setters (C2 replaces them anyway).
6. `writeConfig(raw)` and `_handleResetWriteEffect()` then update `_profile`
   directly — the divergence becomes unrepresentable, and the legacy path gains
   `direction` for free.

This is the largest change in this document (~90 mechanical substitutions) but
it deletes a whole shadow state machine. Do it after C1–C3 land, in its own
commit, with the native suite as the gate.

---

### M2 — Typed alert-limit setters permanently drop `alertConfigState()` to `UNKNOWN`

`src/INA3221.cpp:3074` (`writeRegister16`), and its callers
`setCriticalAlertLimit`, `setWarningAlertLimit`, `setShuntSumLimit`,
`setPowerValidUpperLimit`, `setPowerValidLowerLimit`

Those five typed setters route through the *raw diagnostic* `writeRegister16()`,
which calls `_markRegisterUnknown(reg)` on success. They also update
`_profile.alerts`, so the driver knows exactly what it wrote and what the
desired state now is — yet it reports the alert family as unverifiable:

```
begin                      -> alertState=APPLIED dirty=0
setCriticalAlertLimit(CH1) -> OK, alertState=UNKNOWN dirty=1
                              reason='Hardware register state unknown'
```

`setSummationChannels()` and `setAlertLatchEnable()`, the other two typed alert
setters, correctly use `_markRegisterDirty()`. The inconsistency is accidental.

**Proposed fix.** Give the typed setters a private
`_writeManagedRegister(reg, value)` that writes through
`_writeRegister16Tracked()` and then verifies by readback exactly like C2's
helper, so all typed setters share one certainty discipline. Leave the public
`writeRegister16()` on `_markRegisterUnknown()` — for an arbitrary raw write
that is the honest answer.

---

### M3 — A deadline that expires inside a stage terminalises as `FAILED`, not `TIMED_OUT`

`src/INA3221.cpp:1312`, `:1332`, `:1349`, `:1508`, `:1530`, `:1583` and the
matching branches in `_pollPowerDownOperation`

`pollJob()` re-terminalises only if the stage handler left the job `ACTIVE`:

```cpp
if (st.code == Err::DEADLINE_EXPIRED && _jobState == JobTerminalState::ACTIVE) {
  _finishJob(JobTerminalState::TIMED_OUT, st, ...);
}
```

But every stage handler already called `_finishJob(FAILED | PARTIAL, st, …)` on
`!st.ok()`, so the guard never fires for a deadline detected *inside* a stage.
The same physical condition produces `TIMED_OUT` when noticed at poll entry and
`FAILED`/`PARTIAL` when noticed one line deeper — the owner cannot distinguish
"the device failed" from "I ran out of time", which are different recovery
decisions.

**Proposed fix.** Choose the terminal state in one place:

```cpp
JobTerminalState INA3221::_terminalStateFor(const Status& st, bool writeStage) const {
  if (st.code == Err::DEADLINE_EXPIRED) return JobTerminalState::TIMED_OUT;
  if (writeStage && _writeMayHaveReachedDevice(st)) return JobTerminalState::INDETERMINATE;
  return _jobAnyWriteConfirmed ? JobTerminalState::PARTIAL : JobTerminalState::FAILED;
}
```

and use it at all ~10 `_finishJob(...)` failure sites. That also removes the
repeated `_jobAnyWriteConfirmed ? PARTIAL : FAILED` / `PARTIAL : NONE` ternary
pairs, currently written out eight times.

---

### M4 — No health telemetry is recorded before the first successful initialisation

`src/INA3221.cpp:3118` (`_updateHealth`) and `:3151` (`_recordFailure`)

`_updateHealth()` returns early when `!_initialized`, so every transfer made by
`begin()` — up to 35 of them — is invisible to the health counters.
`_recordFailure()`, used for identity mismatches inside the same job, increments
the counters regardless. The two disagree for the same job:

```
begin() with a dead bus            -> code=I2C_BUS  state=UNINIT
                                      totalFailures=0  lastError=OK
begin() with a wrong Manufacturer  -> totalFailures=1  lastError set
```

So the one situation where a bring-up diagnostic is most useful — the bus is
dead — leaves `lastError()` empty. The README says only that "validation and
precondition failures do not count as transport-health failures", which implies
transport failures do.

**Proposed fix.** Drop the `!_initialized` early-return from `_updateHealth()`
and keep the guard only where it belongs — the `DriverState` transition, which
should not leave `UNINIT` before a successful initialisation:

```cpp
Status INA3221::_updateHealth(const Status& st) {
  if (st.inProgress()) return st;
  const uint32_t nowMs = _nowMs();
  if (st.ok()) {
    _lastOkMs = nowMs;
    _consecutiveFailures = 0;
    if (_totalSuccess < UINT32_MAX) ++_totalSuccess;
    if (_initialized) _driverState = DriverState::READY;
  } else {
    _lastErrorMs = nowMs;
    _lastError = st;
    if (_consecutiveFailures < UINT8_MAX) ++_consecutiveFailures;
    if (_totalFailures < UINT32_MAX) ++_totalFailures;
    if (_initialized) {
      _driverState = _consecutiveFailures >= _transport.offlineThreshold
                         ? DriverState::OFFLINE : DriverState::DEGRADED;
    }
  }
  return st;
}
```

`_recordFailure()` then becomes the non-transport half of the same function;
consider collapsing the two. `begin()` calls `unbind()` on failure and `unbind()`
does not reset the counters, so `lastError()` survives for the caller — check
`test_failed_first_begin_leaves_driver_unbound` and
`test_begin_success_sets_ready_and_counters` still hold.

---

### M5 — A confirmed software reset fabricates alert evidence

`src/INA3221.cpp:3310` (`_handleResetWriteEffect`)

```cpp
if (confirmed) {
  _retainedAlerts.raw = 0;
  _retainedAlerts.powerValid = false;
  _retainedAlerts.timingControl = false;
  _retainedAlerts.conversionReady = false;
  ...
```

The post-reset Mask/Enable value is `0x0002` (TCF set), not `0`, and the driver
did not read the register — it is publishing an observation it never made.
`AlertSnapshot::raw` is documented as "raw value from the consuming read". The
sibling policy elsewhere in the same file is the opposite:
`_readMaskEnableWithTimeout()` deliberately sets `evidenceUncertain` rather than
guess.

**Proposed fix.** Do not invent a reading. Clear what the reset genuinely
invalidates and leave the snapshot empty until the next real read:

```cpp
if (confirmed) {
  _retainedAlerts = AlertSnapshot{};   // no observation survives the reset
  _maskEnableWritableCache = 0;        // hardware is back to POR writable bits
  ...
}
```

Document in `softReset()` that retained alert evidence is discarded because the
device state it described no longer exists.

---

### M6 — The CVRF-low recheck polls the bus roughly every 2 ms until the deadline

`src/INA3221.cpp:1531` (`_pollSampleOperation`, `SAMPLE_READ_MASK`)

When CVRF is low the engine arms a flat 1 ms rewait and reads Mask/Enable again.
With a device whose CVRF never asserts and a 100 s deadline that is ~49 500
destructive Mask/Enable reads — measured on the host — each of which also
consumes alert evidence. On a shared bus that is a lot of traffic for a fault
path. The README documents the behaviour ("bounded by the absolute deadline and
owner poll cadence") but not its rate.

**Proposed fix.** Back the recheck interval off toward the timescale the flag
actually moves on:

```cpp
// First recheck stays fast (the conversion may have just finished); later ones
// back off to at most an eighth of the maximum cycle, capped at 50 ms.
const uint64_t backoffMs = _jobWaitDurationMs / 8U;
_jobWaitDurationMs = backoffMs < 1U ? 1U : (backoffMs > 50U ? 50U : backoffMs);
```

`test_owner_permanently_low_cvrf_waits_until_owner_deadline` needs its expected
transfer count updated.

---

### M7 — The ESP-IDF example only builds when the checkout directory is named `INA3221`

`examples/esp_idf/basic/CMakeLists.txt:3`, `examples/esp_idf/basic/main/CMakeLists.txt:4`

`EXTRA_COMPONENT_DIRS` points at the repository root, so the component takes its
name from the checkout directory, while `main/CMakeLists.txt` hard-codes
`REQUIRES INA3221`. CI works only because the container mounts the repo at
`/INA3221`. A clone into `ina3221`, `INA3221-driver` or a git worktree
directory fails to configure, with an error that does not point at the cause.

**Proposed fix.** Stop depending on the directory name. The robust option is a
thin `components/INA3221/CMakeLists.txt` inside the repository that registers
the parent's `src/`+`include/`, with the example pointing
`EXTRA_COMPONENT_DIRS` at `components/`. The cheap option is to document the
requirement prominently in `examples/esp_idf/basic/README.md` and
`docs/IDF_PORT.md`, and to add a `message(FATAL_ERROR ...)` when the resolved
component is absent. At minimum, do the documentation — a silent configure
failure is a poor first experience for exactly the "drop into a bigger
firmware" use case this library targets.

---

### L1 — `encodeSignedField()` does not clamp out-of-range floats

`src/INA3221.cpp:140`

```cpp
long scaled = lrintf(value / lsb);
if (scaled < minValue) scaled = minValue;
else if (scaled > maxValue) scaled = maxValue;
```

`lrintf()` on a value outside `long` range raises `FE_INVALID` and returns an
**unspecified** result, so the clamp runs on garbage. `long` is 32-bit on the
Windows/MinGW toolchain used here, so any shunt input beyond about ±85 V is
affected. Observed:

```
mvToShuntRaw(+1e9 mV) = 0x0000 -> decodes as 0.000 mV
mvToShuntRaw(-1e9 mV) = 0x0000 -> decodes as 0.000 mV
mvToShuntRaw(+200 mV) = 0x7FF8 -> 163.800 mV   (correct)
```

The header documents "Encoded value clamped to the signed 13-bit data range".
A different toolchain may return `LONG_MIN` and produce full-scale *negative*
for a large positive input. These are public static helpers, so an application
can reach this.

**Proposed fix.** Clamp in the float domain, before the conversion:

```cpp
const float minScaled = static_cast<float>(minValue) * lsb;
const float maxScaled = static_cast<float>(maxValue) * lsb;
if (value < minScaled) value = minScaled;
else if (value > maxScaled) value = maxScaled;
long scaled = lrintf(value / lsb);
```

---

### L2 — Dead range guard in the power-valid limit setters

`src/INA3221.cpp:2707`, `:2740`

```cpp
const uint16_t encoded = static_cast<uint16_t>(raw) & kPowerValidLimitWritable; // 0x7FF8
int32_t milliVolts = 0;
(void)decodeBusMilliVolts(encoded, milliVolts);
if (milliVolts < 0 || !validatePowerValidWindow(...).ok()) { ... }
```

`kPowerValidLimitWritable` clears bit 15, so `decodeBusMilliVolts()` can never
produce a negative value and `milliVolts < 0` is unreachable. The masking is
correct — bit 15 of both power-valid limit registers is read-only per Tables
7-38 and 7-40 — so the guard is simply dead. Delete it, or keep it with a
comment saying it is defensive; do not leave it looking load-bearing.

---

### L3 — `begin()` rejects a zero shunt resistance on *disabled* channels

`src/INA3221.cpp:619` (`_legacyToContracts`)

The loop requires `isPositiveFinite(config.shuntResistance[i])` for all three
channels, while `_validateProfile()` requires a non-zero calibration only for
*enabled* channels. `Config`'s defaults are `0.1f` so it rarely bites, but an
application that zeroes the unused entries gets `INVALID_CONFIG` from `begin()`
and success from the equivalent `bind()`. Align `_legacyToContracts()` with
`_validateProfile()`: validate only channels present in
`profile.enabledChannels`.

---

### L4 — Public `Err` values the library never produces

`include/INA3221/Status.h`

`TIMEOUT`, `BUSY`, `DEVICE_OFFLINE` and the `MEASUREMENT_NOT_READY` alias are
never returned by any code path. `BUSY` ("Device is busy with conversion") was
superseded by `CONVERSION_BUSY`, `TIMEOUT` by `DEADLINE_EXPIRED`/`I2C_TIMEOUT`,
and `DEVICE_OFFLINE` is documented as a "passive diagnostic state value" but no
API exposes it as a `Status`.

Removing enumerators shifts the values of everything after them, so this is a
major-version change. For 3.x, mark them in the Doxygen as reserved and not
produced by this library, and mark `I2C_ERROR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`
and `I2C_BUS` explicitly as transport-supplied. Drop the unused ones in the next
major release.

---

### L5 — Owner-job members written but never read

`include/INA3221/INA3221.h` — `_jobReadBusNext`, `_jobStatus`,
`_jobHardwareEffect`

`_jobReadBusNext` is assigned in `_resetOperationState()` and in
`SAMPLE_READ_CHANNELS` but never read — the channel walker recomputes the next
register from `shuntValid`/`busValid` each time. `_jobStatus` and
`_jobHardwareEffect` are only ever read back through `_pendingJobResult`, which
`_finishJob()` fills from the same values. Delete `_jobReadBusNext`; fold the
other two into `_pendingJobResult` unless a future accessor needs them.

---

### L6 — Mask/Enable setters compose their write from a hardware observation cache

`src/INA3221.cpp:2810` (`setSummationChannels`), `:2834` (`setAlertLatchEnable`)

Both build the outgoing value from `_maskEnableWritableCache`, which is only
refreshed by a Mask/Enable **read**. After a path that changes the register
without a following read, the setter writes the stale bits back:

```
begin                                   -> cache=0x0000, device=0x0002
writeRegister16(REG_MASK_ENABLE,0x7C00) -> device=0x7C02, cache still 0x0000
setAlertLatchEnable(true,false)         -> device=0x0802   (SCC1/2/3 and CEN cleared)
```

This is **not** silent and **not** undocumented: `writeRegister16()` sets
`alertConfigState() == UNKNOWN` and `hardwareConfigDirty()` with detail `0x0F`,
the header says raw writes bypass the typed cache helpers, the README says to
reconcile after a diagnostic write, and
`test_raw_cached_register_write_and_reset_remain_dirty_until_recover` pins the
cache-stays-zero behaviour deliberately. An integrator following the documented
contract is warned. It is listed here only because using a *hardware
observation cache* as the source for a *desired-state write* is fragile —
`softReset()` produces the same shape by zeroing the cache while
`_profile.alerts` still holds the configured summation channels.

**Optional improvement** (not a bug fix): build the register from the desired
state both setters already maintain, which is the same projection
`_desiredRegister()` index 8 computes — factor it out and have both call it:

```cpp
uint16_t INA3221::_desiredMaskEnableWritableBits() const {
  uint16_t bits = 0;
  if ((_profile.alerts.summationChannels & CHANNEL_1) != 0U) bits |= cmd::MASK_SCC1;
  if ((_profile.alerts.summationChannels & CHANNEL_2) != 0U) bits |= cmd::MASK_SCC2;
  if ((_profile.alerts.summationChannels & CHANNEL_3) != 0U) bits |= cmd::MASK_SCC3;
  if (_profile.alerts.warningLatch)  bits |= cmd::MASK_WEN;
  if (_profile.alerts.criticalLatch) bits |= cmd::MASK_CEN;
  return bits;
}
```

`_maskEnableWritableCache` then means only what its name says — the last
observed hardware value — and the existing tests keep passing unchanged.

---

### L7 — `powerDown()` makes power-down the *retained desired* state; say so

`src/INA3221.cpp:790` (`startPowerDown`), `:1051` (`_finishJobSuccess`)

A successful power-down commits `_pendingProfile` (with `mode = POWER_DOWN`)
into `_profile` and bumps `profileGeneration()`. This is the tested, intended
contract — `test_power_down_returns_status_and_keeps_driver_initialized`
asserts `getMode() == POWER_DOWN` on success and its sibling asserts the mode is
preserved only on the *failure* path — and `startApplyProfile()` (which needs
only `_initialized`) is the wake-up path. Nothing is stuck.

What is missing is the statement of the consequence. Because power-down becomes
the retained desired state, `startReconcile()` and `recover()` — the two calls
whose Doxygen says they "reapply and verify the retained desired profile" —
re-apply power-down:

```
after begin : profile.mode=SHUNT_BUS_CONT
powerDown   : profile.mode=POWER_DOWN   gen=2  initialized=1
recover     : profile.mode=POWER_DOWN   device CONFIG=0x7120 (mode 000)
```

An integrator with a periodic supervisory `startReconcile()` will read
`recover()`'s "reinitialize and verify the retained profile" and expect it to
restore measurement.

**Proposed fix** (documentation only): add to `powerDown()` and
`startPowerDown()` Doxygen, and to the README job-lifecycle section — *"A
successful power-down replaces the retained desired mode with `POWER_DOWN`.
`startReconcile()` and `recover()` will therefore re-apply power-down; wake the
device with `startApplyProfile()` carrying a measurement mode."* Consider
whether `measurementConfigState()` should read `APPLIED` while the ADC is off;
it is literally true (the register matches the desired profile) but it lets
`startTriggeredSample()` fail later on the mode check rather than on certainty.

---

### L8 — Smaller contract drift

| Item | Detail |
|---|---|
| `readBlocking()` `captureUptimeMs` | The job is driven on a call-relative clock (`_nowMs() - startMs`), so the committed `SampleBatch::captureUptimeMs` is milliseconds-since-this-call, not the caller's monotonic time as `SampleBatch` documents. Either add `startMs` back before committing, or document the exception. |
| README compatibility table | "Direct raw/scaled read or setter — normally one callback" understates triggered-mode reads, which also perform the Mask/Enable readiness read: two callbacks minimum, three for `readChannel()`. C2 adds one more to every setter. |
| README transport section | "`nowMs` and `cooperativeYield` exist for legacy blocking helpers" — `nowMs` also drives `lastOkMs()`/`lastErrorMs()`, which the production path reports through `getSettings()` and the health accessors. |
| `check_core_timing_guard.py` | `strip_non_code()` removes comments before string literals, so a `//` inside a string can hide a following `millis(` on the same line. Swap the two substitutions. |
| Untracked build leftovers | `INA3221-2.0.0.tar.gz` (a `pio pkg pack` artifact from the previous major version) and `docs/doxygen/` still contain pages for files deleted in `51bc893` (`md__a_g_e_n_t_s.html`, `md_docs_2_i_d_f___p_o_r_t___i_m_p_l_e_m_e_n_t_a_t_i_o_n.html`). Both are `.gitignore`d, so they affect only this working tree — delete the tarball and regenerate Doxygen. |

---

## 4. Checked and found correct

Recorded so the next audit does not re-open them.

**Datasheet conformance**

- Every register address, POR value and bit mask in `CommandTable.h`.
- Sign extension and scaling for shunt (13-bit `[15:3]`, 40 µV), bus (13-bit
  `[15:3]`, 8 mV) and shunt-sum (15-bit `[15:1]`, 40 µV), including boundary
  codes `0x8000` / `0x7FF8` / `0x7FFE`.
- `kShuntLimitWritable` `0xFFF8`, `kShuntSumLimitWritable` `0xFFFE`,
  `kPowerValidLimitWritable` `0x7FF8` (bit 15 read-only), `kMaskEnableWritable`
  (bit 15 reserved; flag bits are not clearable by writing, §7.6.2.16).
- `convTimeUs` / `convTimeMaximumUs`: exact match with the `tCONVERT` typical
  and maximum columns of the electrical table.
- `maximumCycleTimeUs()` = per-channel conversion sum × enabled channels ×
  averaging samples, with correct overflow guards; the README's 50 651 136 µs
  typical / 55 713 792 µs maximum figures check out.
- `maximumJobTransfers()` and the README's 35/33/33/8/7/3 table.
- `AlertProfile`/`DeviceProfile` defaults reproduce the datasheet POR values
  (`0x7FF8`, `0x7FFE`, `0x2710`, `0x2328`).

**Behaviour that looks wrong but is not**

- *Triggered-sample deadline rejection happens on the first `pollJob()`, not at
  `startTriggeredSample()`.* Still bus-silent (`transfers = 0`), which is what
  the README claims.
- *Profile jobs write the Configuration register before the alert limits.* The
  ordering is deliberate and pinned by `buildExpectedProfile()` /
  `queueProfileSequence()` in `test_owner_operations.cpp`. It cannot create an
  exposure window that the silicon does not already have: POR `CONFIG` is
  `0x7127` (all channels enabled, continuous shunt+bus) while every crit/warn
  limit is `0x7FF8`, positive full scale, "effectively disabling the alert".
  Reordering would shrink nothing that matters.
- *`check_cli_contract.py`'s word-boundary command check is weak on its own* — a
  deleted dispatch branch does not fail it. But `check_idf_example_contract.py`
  runs in the same `validate-library` CI job, reads the same Arduino
  `main.cpp`, and requires a real dispatch pattern
  (`command_has_dispatch()`) for a strict superset of the same command list.
  The regression cannot ship. Leave both as they are.
- *`bind()`'s `@note` about clearing prior job results.* It reads as a
  contradiction with the `RESULT_PENDING` guard but describes the state a
  successful call leaves behind, which is accurate.
- *ESP-IDF adapter error mapping.* `ESP_ERR_INVALID_RESPONSE` cannot reliably
  identify the NACK phase of a combined transfer; mapping it to a general I2C
  error while preserving `esp_err_t` in `Status::detail` is the documented and
  correct choice (`docs/IDF_PORT.md`).
- *Writing `0` to the Mask/Enable flag bits during a profile apply.* Harmless:
  the datasheet states writing the register does not clear flag status.
- *`Doxyfile` `EXAMPLE_PATH` with no `@include`.* Harmless configuration.

---

## 5. Suggested order of work

The findings collapse into three root causes plus a set of independent fixes.

**Root cause A — the compatibility facade keeps a second copy of device state.**
C2, H1, M1, M2. Fix M1 first (single source of truth), then C2
(`_applyConfigVerified`); H1 and M2 largely fall out.

**Root cause B — outcomes are classified by inspecting status codes.**
H3, H5, M3. Fix H3 (`_lastTransferAttempted`), then M3 (`_terminalStateFor`)
and H5 follow.

**Root cause C — device semantics.** H2 (TCF polarity), H4 (shunt-sum
preconditions), M5 (fabricated alert evidence), M6 (CVRF backoff).

**Independent:** C1, C3, M4, M7, L1–L8.

A reasonable sequence:

1. **C1, C3** — the two "the library is stuck or returns the wrong answer"
   defects. Small, local, highest value for bench use.
2. **H2, H4, M5** — device-semantics correctness.
3. **H3 → M3 → H5** — the classification refactor.
4. **M1 → C2 → M2 → H1** — the single-source-of-truth refactor.
5. **M4, M6, M7, L1–L8** — cleanup and documentation.

Each step should keep `pio test -e native`, `tools/check_*.py`,
`scripts/generate_version.py check` and `doxygen Doxyfile` green, and add a
regression test for the behaviour it changes. C1, C2, C3, H1, H2 and H4 have no
current test coverage — the native suite and the HIL suite both accommodate
them rather than catching them, so add the missing tests as part of each fix.
