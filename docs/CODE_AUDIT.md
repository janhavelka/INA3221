# INA3221 code audit — findings, resolutions, and verification

| | |
|---|---|
| Audited | 2026-08-27, library `3.1.0` at `9c18102` (the `v3.1.0` tag) |
| Datasheet reference | TI SBOS576C (May 2012, revised September 2025), [`INA3221_datasheet.pdf`](INA3221_datasheet.pdf) |
| Implemented in | `8dff5f9`, `a41e791`, `1eb94c7` |
| Re-verified | 2026-09-04 against `1eb94c7` |
| Result | 22 audit findings + 3 follow-up defects — all implemented; each re-verified against the baseline claim and the current behaviour |

## Purpose and how to read this

This is a **verification checklist, not a work list**. Every finding below is
already implemented. An independent reviewer should use it to confirm the
implementation is correct, not to re-derive the defect.

Each entry gives:

- **Defect** — what was actually wrong at `9c18102`, restated after re-checking
  the original audit text against the baseline code;
- **Resolution** — what was done, including where it deliberately diverged from
  the audit's original proposal and why;
- **Where** — the code that implements it at `1eb94c7`;
- **Verify by** — the cheapest way to convince yourself it is right.

The permanent contract changes are recorded in [`../CHANGELOG.md`](../CHANGELOG.md),
[`../README.md`](../README.md) and the Doxygen in
[`../include/INA3221/`](../include/INA3221/INA3221.h). This file exists only so
that verification does not have to start from scratch; delete it once reviewed.

Host reproductions referenced below compile standalone:

```
c++ -std=c++17 -w -I include repro.cpp src/INA3221.cpp -o repro
```

with a fake transport modelling the INA3221 power-on register defaults and
read-clear Mask/Enable behaviour.

## Status summary

| ID | Finding | Severity | Status |
|---|---|---|---|
| C1 | `readBlocking()` poll loop bounded by iteration count, not time | critical | Fixed, verified |
| C2 | Typed configuration setters blocked all measurement reads | critical | Fixed, verified |
| C3 | Outstanding legacy conversion locked out the whole API | critical | Fixed, verified |
| H1 | Legacy float path ignored `ShuntCalibration::direction` | high | Fixed, verified |
| H2 | Timing-control fault could never be reported | high | Fixed, verified |
| H3 | Transfer outcomes classified by status code, not by callback invocation | high | Fixed, scope narrowed by design |
| H4 | Shunt-sum reads returned stale data as valid | high | Fixed, verified |
| H5 | Arduino adapter rejected deadlines tighter than the Wire timeout | high | Fixed, verified |
| M1 | `_config` and `_profile` were two sources of truth | medium | Resolved differently by design |
| M2 | Typed alert setters dropped `alertConfigState()` to `UNKNOWN` | medium | Fixed, verified |
| M3 | Interior deadline terminalised `FAILED` instead of `TIMED_OUT` | medium | Fixed, verified |
| M4 | No health telemetry before first successful initialisation | medium | Fixed, verified |
| M5 | Confirmed software reset fabricated alert evidence | medium | Fixed, remedy narrowed by design |
| M6 | CVRF-low recheck polled the bus every ~2 ms | medium | Fixed, verified |
| M7 | ESP-IDF example only built from a directory named `INA3221` | medium | Fixed, verified |
| L1 | `encodeSignedField()` did not clamp out-of-range floats | low | Fixed, verified |
| L2 | Dead range guard in the power-valid limit setters | low | Fixed, verified |
| L3 | `begin()` rejected zero shunt resistance on disabled channels | low | Fixed, verified |
| L4 | Public `Err` values the library never produces | low | Documented, values retained |
| L5 | Owner-job members written but never read | low | Fixed, verified |
| L6 | Mask/Enable setters composed writes from an observation cache | low | Fixed, verified |
| L7 | `powerDown()` makes power-down the retained desired state | low | Documented |
| L8 | Smaller contract drift (7 items) | low | Fixed, verified |
| F1 | Mask/Enable verification readback consumed CVRF | high | Fixed, verified |
| F2 | Timing checker defeated by C++ digit separators | medium | Fixed, verified |
| F3 | Transfer-budget cap applied at 2 of 3 Arduino poll sites | medium | Fixed, verified |

F1-F3 are follow-up defects introduced or left by the first two fix commits;
they were found in a second review pass and fixed in `1eb94c7`.

---

## Critical

### C1 — `readBlocking()` bounded its poll loop by iteration count, not time

**Defect.** The loop budget was `timeoutMs + maximumTransfers * 3 + 4`
iterations, but nothing tied one iteration to one millisecond. A triggered
sample must wait a full maximum conversion cycle: 8 ms for the default
three-channel shunt+bus AVG_1 profile, 30 ms at AVG_4, and 7.4 s at AVG_1024
with the conversion times still at their 1.1 ms default. (Pushing VSHCT and
VBUSCT to 8.244 ms as well reaches 55.7 s — the figure the README quotes — but
that is a different profile, not the default.) Any host that spins faster than
roughly 150 iterations per millisecond exhausted the budget first and returned
`CANCELLED`. That is every ESP32-class target.

The native tests passed because `FakeBus::advanceOnYieldMs = 1` advanced the
fake clock a whole millisecond per iteration. The HIL suite passed because the
only steps that invoke `readBlocking()` — `DATA-003` and `DATA-003X`, the two
`read` commands — run with continuous mode already in effect from
`OWNER-037 "mode sbc"`, where no conversion wait exists.

**Resolution.** The loop now runs against the absolute extended monotonic
deadline. `pollJob()` already enforces the deadline itself, so the only
remaining hazard is a clock that never advances; a `STALLED_CLOCK_SPIN_LIMIT`
guard covers that and resets on either time or transfer progress. Capture uptime
is published in the caller's absolute domain rather than call-relative, which
also closed the `captureUptimeMs` item from L8.

**Where.** `readBlocking()` at `src/INA3221.cpp:2272`, spin guard at `:2334`;
`STALLED_CLOCK_SPIN_LIMIT` in `include/INA3221/INA3221.h`.

**Verify by.** Driving `readBlocking()` in triggered mode with a clock that
advances once per N loop iterations. Measured at `1eb94c7`:

| loop iterations per simulated ms | result |
|---|---|
| 1 | `OK` |
| 1 000 | `OK` |
| 20 000 (ESP32-class) | `OK` |
| 20 000, AVG_4 (~30 ms wait) | `OK` |
| 20 000, AVG_64 (~465 ms wait) | `OK` |
| 20 000, across the 32-bit wrap | `OK`, capture uptime `4294967031` |

A genuinely stalled clock still returns `CANCELLED`, which is the designed
behaviour. Tests: `test_read_blocking_tolerates_many_spins_per_millisecond`,
`test_read_blocking_stall_guard_cancels_feasible_wait`.

### C2 — Every typed configuration setter blocked all measurement reads

**Defect.** `setMode`, `setAveraging`, `setVBusConvTime`, `setVShuntConvTime`,
`setChannelEnable` and both `startConversion` overloads performed a successful,
fully specified Configuration write and then unconditionally called
`_markRegisterDirty(REG_CONFIG)`, dropping `measurementConfigState()` from
`APPLIED` to `DIRTY`. `_ensureMeasurementReadyForRead()` requires `APPLIED`, so
every subsequent direct read failed with `CONFIG_UNKNOWN` until a full
`recover()`. The documented legacy single-shot flow could therefore never
complete.

The HIL suite hid this by inserting a bare `recover` (`BASE-008`) after its
configuration steps.

**Resolution.** All typed Configuration and alert mutations share one
write/readback verifier. Candidate profile state commits only after a matching
read, so a verified setter leaves `APPLIED` and a mismatch or transport failure
leaves honest `DIRTY`/`UNKNOWN`. Reading the Configuration register is
side-effect-free by datasheet §7.6.2.1, so the readback is safe even directly
after a trigger write.

**Where.** `_writeManagedRegisterVerified()` and `_applyConfigVerified()` in
`src/INA3221.cpp:3124`/`:3166`; all setters route through them.

**Verify by.** `begin()` → `setAveraging(AVG_16)` → `readShuntVoltage()`. At
`9c18102` the read returns `CONFIG_UNKNOWN`; at `1eb94c7` certainty stays
`APPLIED` and the read returns `OK`. Cost is now two callbacks per setter
(write + readback), recorded in the README compatibility table. Test:
`test_typed_config_setter_requires_matching_readback`.

### C3 — An outstanding legacy conversion locked out the whole API

**Defect.** `startConversion()` set `_conversionStarted`, cleared only by
observing CVRF, by another Configuration write, by a reset, or by
`unbind()`/`end()`. If CVRF never arrived — a device fault, a lost destructive
read, another bus master consuming the flag — the object was unusable:
`startInitialize`, `startPowerDown`, `bind()` and `recover()` all returned
`CONVERSION_BUSY`, and `cancelJob()` had nothing to cancel. `bind()` refused
even though its first action is `unbind()`, which would have cleared the flag.

No test asserted `CONVERSION_BUSY`, so nothing depended on the behaviour.

**Resolution.** Rebind and the lifecycle/recovery jobs (`INITIALIZE`,
`APPLY_PROFILE`, `RECONCILE`, `POWER_DOWN`) discard stale legacy conversion
bookkeeping bus-silently, because each rewrites the Configuration register
anyway. Sample jobs still reject mixed ownership, which is the case where
silently proceeding would produce a sample of unclear provenance. A new
bus-silent `cancelConversion()` gives an explicit escape, exposed as `cancel` in
both CLIs.

**Where.** `bind()` guard removed at `src/INA3221.cpp:533`; `_startJob()`
narrowed at `:675`; `cancelConversion()` at `:2103`;
`_clearLegacyConversionState()` at `:566`.

**Verify by.** Trigger a conversion with a device that never asserts CVRF, then
confirm `startInitialize` is admitted, `cancelConversion()` returns `OK` with
zero transport calls, and a sample job is still rejected. Tests:
`test_legacy_conversion_can_be_cancelled_or_abandoned_for_recovery`,
`test_successful_rebind_discards_active_legacy_conversion_bus_silently`.

---

## High

### H1 — The legacy float path ignored `ShuntCalibration::direction`

**Defect.** `calculateCurrentMilliAmps()` honours
`CurrentDirection::POSITIVE_SHUNT_IS_NEGATIVE_CURRENT`; `readCurrent()`,
`readPower()` and `readChannel()` computed `shuntMv / rShunt` with no sign term.
For a channel bound with the inverted convention, the owner path and the float
path reported **opposite signs for the same physical current**. The root cause
was that `_syncLegacyConfigFromProfile()` copied only `resistanceMicroOhms`, and
the legacy `Config` had no direction field at all, so the convention was not
even expressible from `begin()`.

**Resolution.** All three float readers apply the profile direction, and
`Config` gained `CurrentDirection direction[3]` that `begin()` maps onto
`DeviceProfile::shunts[i].direction` and `getConfig()` reports back. The audit
offered two remedies — routing through the fixed-unit helper, or a float sign
flip — and warned that the first would round to whole milliamps. The
implementation took the second, preserving float precision. **This is the better
choice** and the audit's preference is superseded.

**Where.** `src/INA3221.cpp` `readCurrent`/`readPower`/`readChannel`;
`_legacyToContracts()` `:601`; `_syncLegacyConfigViewFromProfile()` `:577`;
`Config::direction` in `include/INA3221/Config.h`.

**Verify by.** Bind with `{100000 µΩ, POSITIVE_SHUNT_IS_NEGATIVE_CURRENT}` and a
shunt register of `0x2710` (+50 000 µV over 0.1 Ω). Both paths must report
−500 mA / −5000 mW. Confirmed at `1eb94c7` through both `bind()` and
`begin(Config)`. Tests: `test_float_measurement_helpers_apply_profile_direction`,
`test_legacy_config_current_direction_round_trips_through_profile`.

### H2 — The timing-control fault could never be reported

**Defect — restated.** The original audit titled this "TCF polarity inverted".
That wording is wrong and has been corrected here: the decode
`(raw & MASK_TCF) != 0U` was byte-identical before and after the fix, and was
always right. The real defects were that (a) `_retainedAlerts.timingControl` was
a sticky OR, and (b) TCF's power-on reset value is **1**.

Per the datasheet the TC pin is open-drain and pulls **low** to signal the fault
(§7.3.2.4), TCF "corresponds to the status of the TC pin", and its reset value
is 1 (Table 7-36; Mask/Enable POR `0x0002`). So `TCF == 1` means *no fault*, and
the sticky OR made the field permanently `true` from the first read after
power-up — discarding the only transition that matters. A field named
`timingControl` sitting beside `criticalCh1`/`warningCh1` also reads as "alert
asserted", which is the opposite of what it holds.

**Resolution.** `timingControl` is now the latest observed level, and a derived
`timingControlFault` (`!timingControl`) exposes the fault explicitly on both
`AlertFlags` and `AlertSnapshot`. The audit proposed adding a **software** latch
for the fault; the implementation deliberately did not, because the datasheet
states TCF "does not clear after it has been asserted unless the power is
recycled or a software reset is issued" — the device already latches it.
**The implementation is right and the audit's proposal was redundant.**
`timingControlFault` is correctly excluded from the read-clear `events` bitmap,
since TCF is a device-latched condition rather than an event a host read
consumes.

**Where.** `_retainMaskEnable()` `src/INA3221.cpp:877-895`; `_decodeAlertFlags()`
`:907`.

**Verify by.** `AlertSnapshot` must remain trivially copyable (there is a
`static_assert` on `SampleBatch`); both CLIs print `TCF`, `TC_FAULT`,
`TimingControl` and `TimingControlFault`, and both static contract checkers pin
those labels. Note the second-order consequence now documented in the README:
because `startInitialize()` writes the Configuration register, this library
disables the device's timing-control function on every bring-up.

### H3 — Transfer outcomes were classified by status code, not by callback invocation

**Defect.** `_ambiguousWriteFailure()` and the tracked transport wrappers both
decided "did anything reach the bus?" by inspecting the returned `Err`. That
proxy was wrong in both directions. An adapter returning `INVALID_PARAM` for a
real bus failure was not counted as a health failure and its failed write was
declared definitely-not-reached; and `_jobWriteRegister()` returned
`DEADLINE_EXPIRED` *before* calling the transport when the clamped timeout
rounded to zero, which was then classified as an ambiguous write —
`INDETERMINATE` plus `CONFIG_UNKNOWN` for a transfer that never happened.

**Resolution — scope narrowed by design.** The raw wrappers now record
`_lastCallbackInvoked`, and `_writeMayHaveReachedDevice()` requires it. That
fully fixes the bus-silent half: a budget or deadline rejection is now
`TIMED_OUT` with `HardwareEffect::NONE`, zero transfers and untouched certainty.

The other half was resolved by **contract rather than by code**:
`INVALID_CONFIG` and `INVALID_PARAM` remain "callback-local validation, no bus
access", and that is now a documented obligation on adapters in
`Config.h`, the README transport section and `AGENTS.md`. The reasoning is that
callback invocation alone cannot prove a physical transfer occurred *inside* an
adapter, so the driver cannot infer the phase — the adapter must report it. The
bundled Arduino adapter was updated to match (its tighter-timeout preflight
returns `INVALID_CONFIG`, not a fabricated `I2C_TIMEOUT`), and `check_cli_contract.py`
pins that status. **This is a defensible narrowing, not an incomplete fix**, but
it does mean an adapter that mislabels a bus failure as `INVALID_PARAM` will
still under-report health.

**Where.** `_lastCallbackInvoked` `src/INA3221.cpp:2984`/`:3003`;
`_writeMayHaveReachedDevice()` `:966`; `_finishJobFailure()` `:1076`.

**Verify by.** A profile job polled with `maxTransfers = 8` and 1 ms of deadline
left must terminalise `TIMED_OUT` / `NONE` / 0 transfers with
`measurementConfigState()` still `APPLIED`. Test:
`test_owner_interior_deadline_is_bus_silent_and_terminalizes_timed_out`.

### H4 — Shunt-sum reads returned stale data as valid

**Defect.** `readShuntSumRaw()`/`readShuntSumVoltage()` checked only
`_initialized` and measurement readiness. The Shunt-Voltage Sum register is
filled from single shunt conversions of the channels selected by SCC1-3 and
updated after each complete cycle of those channels (§7.6.2.14), so in a
bus-only mode, or with no summation channel selected, the read returned a stale
or zero value with `OK`. Every other measurement read was gated by
`validateMeasurementRead()`; the sum read was the one left out.

**Resolution.** Both now require a shunt-measuring mode, at least one selected
summation channel, and verified alert configuration, all rejected before I2C. A
dormant summation selection is deliberately still *valid* in a bus-only or
power-down desired profile, so retained future configuration is not lost — only
the read is refused. That rationale is now in the header Doxygen.

**Where.** `src/INA3221.cpp` `readShuntSumRaw()`. Test:
`test_shunt_sum_rejects_bus_only_modes_without_i2c`.

### H5 — The Arduino adapter rejected deadlines tighter than the Wire timeout

**Defect.** `TwoWire` exposes a bus-level timeout, not a per-call one, so the
example adapter refused any callback whose deadline was tighter than the
configured value — reasonable in itself. But `_clampedTransferTimeout()` divides
the remaining deadline by `maxTransfers`, and the CLI advertised
`job step <0..255>` against a 5 000 ms job window: at `maxTransfers = 255` the
share is 19 ms against a 50 ms Wire timeout, so every callback was refused. The
refusal was also reported as `I2C_TIMEOUT`, which counted as a health failure
and marked the register `UNKNOWN`.

**Resolution.** The refusal is now `INVALID_CONFIG` (bus-silent under H3), and
every Arduino owner poll site caps the budget through one shared
`wireSafeTransferBudget()` helper — a zero budget issues no callback and lets
the job end through its deadline. Native ESP-IDF sites are deliberately
uncapped, because that adapter honours a true per-call timeout.

**Where.** `examples/common/I2cTransport.h:76`;
`wireSafeTransferBudget()` `examples/01_basic_bringup_cli/main.cpp:759`, used at
all three `PollContext` sites. `check_cli_contract.py` pins both the status and
the helper-per-site invariant.

---

## Medium

### M1 — `_config` and `_profile` were two sources of truth

**Defect.** Every field of `_config` was already present in `_transport` +
`_profile`, yet both were mutated independently and could diverge — after a raw
`writeConfig()` or a `softReset()` they described different devices. The
sharpest consequence: `_readConversionReadyAt()` decided *whether* to gate on a
conversion delay from `_config.mode` but computed *how long* from `_profile`, so
after a raw write it opened the readiness gate after 8 ms for a conversion
actually needing ~16.9 s typical / ~18.6 s maximum, burning destructive
Mask/Enable reads polling for a CVRF that could not yet be set.

(The original audit put that second figure at 1.24 s. That was wrong: it used
the 1.1 ms conversion-time maximum instead of the 8.244 ms one the raw write
selects, and counted one quantity instead of shunt+bus. For `writeConfig(0x4FFB)`
— CH1 only, AVG_1024, 8.244 ms bus and shunt, SHUNT_BUS_TRIG — the cycle is
`(8244+8244) x 1 x 1024` = 16 883 712 µs typical and `(9068+9068) x 1 x 1024`
= 18 571 264 µs maximum. The 8 ms gate came from the stale desired profile,
`(1210+1210) x 3 x 1` = 7260 µs plus the wake margin, and that figure was
correct.)

**Resolution — resolved differently by design.** The audit proposed deleting the
member and deriving `Config` on demand (~90 mechanical substitutions). The
implementation instead **redefined** it as `_legacyConfigView`, an explicit
*observed hardware* view, keeping `_profile` as the desired state. That
preserves source and API compatibility — including `getConfig()`'s reference
semantics — and keeps raw-register diagnostics meaningful, while making the
divergence semantically correct rather than a bug: after a reset the hardware
*is* at power-on defaults and the desired profile *is* unchanged.

**This is the better trade-off.** The audit's proposal would have been a
breaking change for a problem that was really about naming and about one
inconsistent consumer.

**Where.** `_legacyConfigView` `src/INA3221.cpp:562`;
`_syncLegacyConfigViewFromProfile()` `:577`.

**Verify by.** `writeConfig(0x4FFB)` then `readConversionReady()`: the observed
view must report the raw-written mode and averaging, the desired profile must be
unchanged, and no premature Mask/Enable read may occur. Confirmed at `1eb94c7`:
zero reads after 100 ms for a 1024-average profile. Test:
`test_raw_config_view_survives_host_updates_and_drives_readiness`.

### M2 — Typed alert setters dropped `alertConfigState()` to `UNKNOWN`

**Defect.** The five typed alert-limit setters routed through the raw
diagnostic `writeRegister16()`, which calls `_markRegisterUnknown()` on success —
so a setter that knew exactly what it wrote reported the alert family as
unverifiable. `setSummationChannels()`/`setAlertLatchEnable()` correctly used
`_markRegisterDirty()`; the inconsistency was accidental.

**Resolution.** All seven typed alert mutations share the write/readback
verifier. Note the deliberate asymmetry it implements: the measurement family is
the Configuration register alone, so one verified write restores `APPLIED`; the
alert family spans ten registers, so verifying one setter **preserves the prior
family certainty rather than promoting it**. That rule is now in the README.

**Where.** `_writeManagedRegisterVerified()` `src/INA3221.cpp:3124`, the
`priorState` branch at `:3157`. Test:
`test_typed_alert_setter_requires_matching_readback`.

### M3 — An interior deadline terminalised `FAILED`, not `TIMED_OUT`

**Defect.** `pollJob()` re-terminalised only if the stage handler left the job
`ACTIVE`, but every handler had already called `_finishJob(FAILED | PARTIAL, …)`
on failure. The same physical condition therefore produced `TIMED_OUT` when
noticed at poll entry and `FAILED`/`PARTIAL` one line deeper, so an owner could
not distinguish "the device failed" from "I ran out of time" — different
recovery decisions.

**Resolution.** One central `_finishJobFailure()` maps `DEADLINE_EXPIRED` to
`TIMED_OUT`, distinguishes ambiguous invoked writes, and reports `PARTIAL` only
after a confirmed earlier write. It also removed the eight repeated
`_jobAnyWriteConfirmed ? PARTIAL : FAILED` ternary pairs.

A related deliberate change: a cancelled or timed-out triggered sample no longer
degrades `measurementConfigState()` to `DIRTY`, because the trigger rewrites the
already-verified desired Configuration value. Power-down still does.

**Where.** `_finishJobFailure()` `src/INA3221.cpp:1076`; `cancelJob()` `:1139`.
Tests: `test_owner_interior_deadline_is_bus_silent_and_terminalizes_timed_out`,
`test_owner_triggered_interior_deadline_is_partial_but_keeps_config_applied`.

### M4 — No health telemetry before the first successful initialisation

**Defect.** `_updateHealth()` returned early when `!_initialized`, so all of
`begin()`'s transfers were invisible to the counters, while `_recordFailure()`
(used for identity mismatches in the same job) incremented regardless. The two
disagreed for the same job: a dead bus left `totalFailures() == 0` and
`lastError()` empty, exactly when a bring-up diagnostic is most useful.

**Resolution.** The early return is gone; only the `DriverState` transition
stays gated on `_initialized`, so the driver still cannot report `DEGRADED` or
`OFFLINE` before initialisation succeeds. `begin()` calls `unbind()` on failure
and `unbind()` does not reset the counters, so diagnostics survive.

**Where.** `_updateHealth()` `src/INA3221.cpp:3191`.

**Verify by.** `begin()` against a failing bus: `totalFailures() == 1`,
`lastError() == I2C_BUS`, `state() == UNINIT`.

### M5 — A confirmed software reset fabricated alert evidence

**Defect.** `_handleResetWriteEffect(true)` wrote `raw = 0`, `powerValid =
false`, `timingControl = false`, `conversionReady = false` into the retained
snapshot — an observation the driver never made. The post-reset Mask/Enable
value is `0x0002` (TCF set), not zero, and `AlertSnapshot::raw` is documented as
the value from a consuming read. The sibling policy in the same file is the
opposite: a failed read sets `evidenceUncertain` rather than guessing.

**Resolution — remedy narrowed by design.** The audit proposed clearing the
whole snapshot. The implementation refused, on the grounds that clearing
`events` would **silently acknowledge events the application has not taken** —
which is a worse failure than staleness. A confirmed reset clears the known
power-on writable-bit cache; an ambiguous reset preserves the last real
observation while certainty becomes `UNKNOWN`; retained host events are
untouched until `takeAlertEvents()`. **The implementation is right and the
audit's proposal was wrong**; the reasoning is now in `softReset()`'s Doxygen.

**Where.** `_handleResetWriteEffect()` `src/INA3221.cpp:3388`. Tests:
`test_soft_reset_preserves_real_retained_alert_evidence`,
`test_ambiguous_reset_preserves_observed_mask_enable_cache`.

### M6 — The CVRF-low recheck polled the bus every ~2 ms

**Defect.** A CVRF-low observation armed a flat 1 ms rewait and re-read
Mask/Enable. With a device whose CVRF never asserts and a long deadline that is
tens of thousands of destructive reads, each also consuming alert evidence — a
lot of traffic on a shared bus for a fault path.

**Resolution.** A fixed 50 ms fault recheck, plus a bus-silent wait when no
further bounded read fits before the deadline. The audit proposed a proportional
backoff (an eighth of the cycle, capped); the fixed interval is simpler and its
worst case is easier to state in the README. **Equivalent in effect, simpler in
implementation.**

**Where.** `CVRF_FAULT_RECHECK_MS` used at `src/INA3221.cpp:1561`.

**Verify by.** CVRF held low across a ~99 s deadline: measured 1 942 callbacks at
`1eb94c7` versus 49 497 at `9c18102`. Test:
`test_owner_permanently_low_cvrf_waits_until_owner_deadline`.

### M7 — The ESP-IDF example only built from a directory named `INA3221`

**Defect.** `EXTRA_COMPONENT_DIRS` pointed at the repository root, so the
component took its name from the checkout directory, while `main/CMakeLists.txt`
hard-coded `REQUIRES INA3221`. CI worked only because it mounted the repo at
`/INA3221`. Any other clone name failed to configure, with an error that did not
point at the cause.

**Resolution.** A fixed-name shim at
`examples/esp_idf/basic/components/INA3221/CMakeLists.txt` registers the root
sources and includes; `EXTRA_COMPONENT_DIRS` was removed. CI now mounts at
`/component-source`, so the rename independence is genuinely exercised rather
than asserted. The constraint that still applies to *consumers* integrating the
repository root directly is documented in the README and `IDF_PORT.md`.

**Where.** the shim, `examples/esp_idf/basic/CMakeLists.txt`,
`.github/workflows/ci.yml`. `check_idf_example_contract.py` pins the shim depth
and rejects a CI checkout path named `INA3221`.

---

## Low

### L1 — `encodeSignedField()` did not clamp out-of-range floats

`lrintf()` on a value outside `long` range raises `FE_INVALID` and returns an
**unspecified** result, so the clamp ran on garbage; `long` is 32-bit on the
toolchain used here, so the affected threshold is `LONG_MAX × 0.04 mV`, about
±85,900 V — far outside the device's ±163.8 mV shunt range, which is why this
is rated low. The observed result was `0x0000`, but another toolchain could
return `LONG_MIN` and produce full-scale *negative* for a large positive input.
These are public static helpers taking a caller-supplied float, so an
application can still reach them. Fixed by saturating in the float domain
before the conversion; verified at both extremes and for NaN.

### L2 — Dead range guard in the power-valid limit setters

`kPowerValidLimitWritable` (`0x7FF8`) clears bit 15, which is read-only per
datasheet Tables 7-38/7-40, so `decodeBusMilliVolts()` could never produce a
negative value and the `milliVolts < 0` guard was unreachable. Removed; the
masking itself was correct and is unchanged.

### L3 — `begin()` rejected zero shunt resistance on disabled channels

`_legacyToContracts()` required a finite positive resistance on all three
channels while `_validateProfile()` required it only for *enabled* channels, so
`begin()` and the equivalent `bind()` disagreed. Now aligned: disabled channels
accept any invalid representation — zero, NaN, infinity, and finite
out-of-range values — and are normalised to zero. Test:
`test_begin_ignores_invalid_shunts_on_disabled_channels`.

### L4 — Public `Err` values the library never produces

`TIMEOUT`, `BUSY` and `DEVICE_OFFLINE` are never returned. Removing enumerators
would shift the values of everything after them, so they are **retained** and
documented instead: `BUSY` and `DEVICE_OFFLINE` as reserved legacy values, and
`I2C_ERROR`, `I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`, `I2C_BUS` and
`TIMEOUT` as transport-supplied categories. Removal remains a candidate for the
next major release.

`MEASUREMENT_NOT_READY` is **not** in that group, contrary to the original
audit: it is an alias for the same enumerator value as `CONVERSION_NOT_READY`,
which `_ensureMeasurementReadyForRead()` returns (`src/INA3221.cpp:3358`) on
every triggered-mode read attempted before the conversion completes. The
`Status.h` Doxygen already describes it correctly.

### L5 — Owner-job members written but never read

`_jobReadBusNext`, `_jobStatus` and `_jobHardwareEffect` were assigned and never
read — the channel walker recomputes its next register from
`shuntValid`/`busValid`. `_jobStatus` and `_jobHardwareEffect` had no read
sites at all: `_finishJob()` populates `_pendingJobResult` from its own
parameters rather than from the members. All three removed.

### L6 — Mask/Enable setters composed writes from an observation cache

`setSummationChannels()`/`setAlertLatchEnable()` built the outgoing value from
`_maskEnableWritableCache`, refreshed only by a Mask/Enable *read*. After a raw
write or a software reset that cache was stale and the setter wrote it back,
dropping bits the caller had configured.

The original audit first reported this as a silent clobber and then softened it:
the driver *does* flag it — `writeRegister16()` sets `alertConfigState()` to
`UNKNOWN` with `hardwareConfigDirty()`, the behaviour is documented, and
`test_raw_cached_register_write_and_reset_remain_dirty_until_recover` pins the
cache-stays-zero rule deliberately. The remaining objection was structural:
using a *hardware observation cache* as the source for a *desired-state write*.

Fixed by composing the complete desired register through the profile encoder and
verifying the readback. `_maskEnableWritableCache` now means only what its name
says. The pinning test still passes unchanged.

### L7 — `powerDown()` makes power-down the retained desired state

Not a defect — a documentation gap. A successful power-down commits
`mode = POWER_DOWN` into `_profile` and bumps `profileGeneration()`. That is the
tested, intended contract, and `startApplyProfile()` is the wake-up path. But it
means `startReconcile()` and `recover()`, whose Doxygen promises to "reapply and
verify the retained desired profile", will re-apply power-down. Now stated
explicitly in the header and the README job-lifecycle section.

### L8 — Smaller contract drift

All seven items resolved: `readBlocking()` now publishes capture uptime in the
caller's absolute domain (folded into C1); the README compatibility table
records the two-callback verified setter and the triggered-read Mask/Enable
consumption; the `nowMs` description covers health timestamps; the timing
checker's comment/string ordering hazard was superseded by the full lexer (F2);
`bind()`'s note, the `PollJobSnapshot` cursor fields, and the ignored build
leftovers were all addressed.

---

## Follow-up defects (found after the first fix pass)

### F1 — The Mask/Enable verification readback consumed CVRF

Introduced by C2/M2/L6. Verifying a Mask/Enable write requires reading it back,
and that read is destructive on real silicon. Unlike `readAlertFlags()`, the
verifier did not hand the observed CVRF to the legacy conversion state, so a
typed setter between a trigger and a read left readiness permanently false:

```
mode strig / trig / sumch 1 0 0 / ready  ->  ready = NO   (permanently)
                                shunt 1  ->  CONVERSION_NOT_READY
```

Alert *events* were retained, so no evidence was lost — the readiness was.
Fixed by extracting `_handoffConversionReady()` (`src/INA3221.cpp:900`) and
calling it from both paths; it runs *before* the match check, so even a
verification mismatch still propagates a real observation. Both setters gained
the read-clear `@note` that eight other Mask/Enable methods already carried.
Test: `test_typed_mask_enable_setters_preserve_observed_conversion_ready`.

### F2 — The timing checker was defeated by C++ digit separators

`_quoted_literal_end` treated every `'` as a character-literal opener, so two
separated numeric literals swallowed the code between them:

```cpp
static const unsigned kBusHz  = 400'000;
void f() { unsigned t = millis(); (void)t; }   // -> "Core timing guard PASSED"
static const unsigned kSlowHz = 100'000;
```

Fixed with a token-start gate reusing the mechanism already present for raw
strings, plus odd-pair and balanced-number cases in the adversarial self-test.
Verified: five bypass attempts (odd separators, balanced separators, a stray
apostrophe in a directive, a genuine character literal, a comment marker inside
a string) all now report the hidden call, with no false positives on comments,
strings, raw strings or balanced separators.

### F3 — The transfer-budget cap was applied at 2 of 3 Arduino poll sites

`runOneOwnerSample()` still set `maxTransfers = 1` unconditionally, so
`stress_owner` could hand the Wire callback a sub-50 ms deadline once the
remaining window shrank. All three sites now share `wireSafeTransferBudget()`,
and `check_cli_contract.py` enforces a helper-use-per-`PollContext` invariant so
a fourth site cannot silently skip it.

---

## Checked and found correct

Re-verified against the datasheet and the current code. Recorded so a future
audit does not re-open them.

**Datasheet conformance.** Every register address, power-on-reset value and bit
mask in `CommandTable.h`. Sign extension and scaling for shunt (13-bit `[15:3]`,
40 µV), bus (13-bit `[15:3]`, 8 mV) and shunt-sum (15-bit `[15:1]`, 40 µV),
including boundary codes `0x8000`/`0x7FF8`/`0x7FFE`. The writable masks
`0xFFF8`, `0xFFFE`, `0x7FF8` (bit 15 read-only per Tables 7-38/7-40) and the
Mask/Enable writable mask (bit 15 reserved; flag bits not clearable by writing,
§7.6.2.16). `convTimeUs`/`convTimeMaximumUs` against the `tCONVERT` typical and
maximum columns. `maximumCycleTimeUs()` and the README's 50 651 136 µs typical /
55 713 792 µs maximum figures. `maximumJobTransfers()` and the 35/33/33/8/7/3
table. The `AlertProfile`/`DeviceProfile` defaults against the POR values
`0x7FF8`, `0x7FFE`, `0x2710`, `0x2328`.

**Behaviour that looks wrong but is not.**

- The triggered-sample deadline rejection happens on the first `pollJob()`, not
  at `startTriggeredSample()` — still bus-silent, with `transfers == 0`.
- Profile jobs write the Configuration register *before* the alert limits. The
  ordering is deliberate and pinned by `buildExpectedProfile()` /
  `queueProfileSequence()`, and it cannot create an exposure the silicon does
  not already have: POR `CONFIG` is `0x7127` (all channels enabled, continuous
  shunt+bus) while every crit/warn limit is `0x7FF8`, positive full scale,
  "effectively disabling the alert".
- `check_cli_contract.py`'s word-boundary command check is weak alone, but
  `check_idf_example_contract.py` runs in the same CI job, reads the same
  Arduino `main.cpp`, and requires a real dispatch pattern for a strict superset
  of the same command list.
- `bind()`'s note about clearing prior job results describes the state a
  successful call leaves behind; it is not a contradiction with the
  `RESULT_PENDING` guard.
- The ESP-IDF adapter's `ESP_ERR_INVALID_RESPONSE` mapping: that code cannot
  reliably identify the NACK phase of a combined transfer, so mapping it to a
  general I2C error while preserving `esp_err_t` in `Status::detail` is correct.
- Writing `0` to the Mask/Enable flag bits during a profile apply is harmless —
  the datasheet states writing the register does not clear flag status.

## Corrections made to the original audit text

Recorded for traceability; the entries above already reflect them.

1. **H2 title and framing.** "TCF polarity inverted" was wrong — the decode
   `(raw & MASK_TCF) != 0U` is byte-identical at `9c18102` and `1eb94c7`. The
   defects were the sticky OR and the reset value of 1. The same false claim
   reached `CHANGELOG.md` ("Corrected TCF polarity") and has been removed there.
2. **H2 proposed remedy.** The proposed software latch is redundant: TCF is
   latched by the device until a power cycle or software reset.
3. **M5 proposed remedy.** Clearing the whole retained snapshot would silently
   acknowledge un-taken events; the narrower implemented remedy is correct.
4. **M1 proposed remedy.** Deleting the `Config` member would have been a
   breaking change; redefining it as an observed view addresses the real defect.
5. **H1 preferred remedy.** The audit leaned toward routing the float path
   through the fixed-unit helper, which would have coarsened resolution to whole
   milliamps; the implemented sign flip is better.
6. **H3 scope.** The audit implied the status-code conflation could be fully
   removed in the driver. It cannot: the driver cannot observe what happens
   inside an adapter, so half of it is necessarily an adapter contract.
7. **L6 severity.** Reported first as a silent clobber; the driver does flag the
   condition through `alertConfigState()` and `hardwareConfigDirty()`.
8. **L7 classification.** Reported as a defect, then reclassified as a
   documentation gap after review; power-down retention is the tested contract.
9. **M1 worked example arithmetic.** The audit stated the raw-written profile
   needed a 1.24 s conversion; the correct figures are 16.9 s typical and
   18.6 s maximum. The defect and the 8 ms stale-gate figure were unaffected.
10. **C1 conversion-wait figure.** The audit put the default profile at "~52 s
    at AVG_1024". With the conversion times at their 1.1 ms default that cycle
    is 7.4 s; 55.7 s requires 8.244 ms conversion times as well. The 8 ms and
    30 ms figures were correct.
11. **C1 HIL cross-reference.** The audit named `DATA-003A "mode sbc"` as what
    put the `read` steps in continuous mode. `DATA-003A` actually follows them
    and guards the per-channel direct reads; continuous mode comes from
    `OWNER-037 "mode sbc"`.
12. **L1 affected range.** The audit said "beyond about ±85 V"; the actual
    threshold is `LONG_MAX × 0.04 mV` ≈ ±85,900 V, a factor of 1000 higher.
    That makes the low severity rating more clearly right, not less.
13. **L4 dead-value list.** The audit listed `MEASUREMENT_NOT_READY` among
    values the library never produces. It is an alias of `CONVERSION_NOT_READY`,
    which is produced on a routine path.
14. **L5 precision.** `_jobStatus` and `_jobHardwareEffect` were not "read back
    through `_pendingJobResult`" — they had no read sites whatsoever.
15. **Baseline line citations.** Several of the original audit's
    `src/INA3221.cpp:NNNN` references pointed 6-30 lines away from the symbol
    they named (C3, H1, H2, H3, H4). Every reference in this document was
    re-derived against `1eb94c7` and machine-checked against the symbol it
    names.

## Open items

Neither is an audit finding; both are release tasks surfaced by this work.

- **Version.** `library.json` is still `3.1.0`, the version the `v3.1.0` tag
  already occupies. The public surface has since gained `cancelConversion()`,
  `Config::direction[3]` and `timingControlFault` on two structs, and
  `AlertSnapshot::timingControl` changed meaning. That is a MINOR bump to
  `3.2.0` under the rules in `AGENTS.md`, and the `timingControl` semantics
  change should be called out because it can silently alter an integrator's
  behaviour.
- **Hardware validation.** `tools/hil_cli_runner.py` is unchanged since
  `v3.1.0`: 379 steps, no `cancel` step, no `TCF`/`TC_FAULT` assertions, and no
  transfer-count assertion over a typed setter — which is exactly where
  behaviour changed from one callback to two. The evidence ledger in
  [`HIL.md`](HIL.md) has no entry covering any of this work.
