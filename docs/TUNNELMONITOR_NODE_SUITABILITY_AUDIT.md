# TunnelMonitor-node suitability audit

## INA3221 three-channel power monitor library

Date: 2026-07-18

Audit result: **good protocol base, product decision and focused refactor
required before integration**

INA3221 v2.0.0 is a useful base for a platform power-monitor driver. It is
framework-neutral, uses injected transport, has fixed storage, verifies device
identity, preserves useful transport errors, implements correct signed
measurement decoding, and contains a staged sample engine. TunnelMonitor should
reuse that chip work instead of adding another register protocol to `I2cTask`.

It is not suitable unchanged. The library schedules delay-only reads from
typical conversion times rather than datasheet maximum times. Reads of the
destructive Mask/Enable register can clear alert evidence without retaining it.
Initialization and recovery do not establish or restore a complete device
profile. Staged jobs cannot be cancelled, and the older readiness path can
interfere with the newer job path. Continuous multi-register reads do not
provide an atomic, coherent fixed-unit result for the selected channels. The
library's mandatory `OFFLINE` gate also competes with `I2cTask`, which already
owns device health, deadlines, retries, and bus recovery.

There is also a product-level gate: TunnelMonitor hardware 2.0.0 specifies one
optional INA228 at address `0x41`, not an INA3221. The current result and sample
schema are scalar and INA228-shaped. INA3221 support therefore needs an
explicit board profile, address strap, channel map, shunt values, sampling
profile, alert policy, and data-contract decision. It must not be introduced as
an INA228 alias or by adding parallel INA3221 branches to `I2cTask`.

The recommended path is to refactor the maintained v2 core, release and
exact-pin a new immutable revision, then add one small owner-private INA3221
adapter for a statically selected board profile. Do not keep both a direct and
a library implementation of INA3221 for the same profile. INA228 support can
remain for hardware 2.0.0 or another approved board profile.

## Audit basis

The audit used these exact revisions:

| Repository | Revision | Notes |
| --- | --- | --- |
| TunnelMonitor-node | `fff99fe17e60b9287ec4d8d3eca5b3230ae44223` | Branch `prompt-44b-sequence`; current INA228 implementation and architecture authority |
| INA3221 checkout at audit start | `2b3093b7de04e6a0c4cff4c604e125c1dcd9dc62` | Old `audit/ina3221-industry-readiness-exploration` branch; described as `v1.2.0-3-g2b3093b`; not the integration candidate |
| INA3221 current checkout | `bcf1abb62d453421f12847de873241963c32c79a` | Branch `main`; annotated `v2.0.0` tag target, `origin/main`, and `origin/HEAD`; maintained integration candidate |

Unless stated otherwise, INA3221 source references below mean v2.0.0 at
`bcf1abb`. TunnelMonitor references mean the revision above.

The branch checked out at the start of the audit was materially older than
v2.0.0. It lacked the v2 staged sampling APIs, `tickStatus()`, power-down
support, hardware configuration dirty tracking, and improved probe error
preservation. It was not a suitable production base.

### Latest-branch revalidation

On 2026-07-18, `origin` was fetched again with remote-branch pruning and tags.
`origin/HEAD` selects `origin/main`, and `origin/main` is the newest maintained
remote branch. The old audit branch is six commits behind the main line after
accounting for its two branch-only commits. The working checkout was safely
switched to local `main` and fast-forwarded to
`bcf1abb62d453421f12847de873241963c32c79a`. It is now zero commits ahead and
zero commits behind `origin/main`, and the audit report remains the only
untracked file.

The full source delta from the historical audit-start branch to v2.0.0 was
reviewed, then every hard finding, source reference, severity, and recommended
action was checked again against the final `main` source. The report had
already used this exact v2.0.0 commit in a detached worktree, so no finding or
severity changed during this final revalidation.

Primary device behavior was checked against the bundled Texas Instruments
`docs/INA3221_datasheet.pdf`, document `SBOS576C`, revision C, revised September
2025. PDF pages 11, 12, 17, 28, and 33 through 36 were rendered and visually
reviewed. These pages cover conversion sequencing and timing, conversion-ready
behavior, alert and summation behavior, register formats, read-clear flags, and
identity values. Poppler was not installed, so PyMuPDF was used for rendering.
Temporary page images were removed after review.

This audit changed no firmware or library source, selected no production
dependency, and ran no new physical hardware test. The only intended repository
change is this report. Existing INA3221 HIL evidence was reviewed separately
and is summarized below.

## Decision summary

### Product decisions required first

TunnelMonitor must decide these points before the adapter or public contract is
implemented:

1. Decide whether INA3221 replaces INA228 on a new board profile, coexists on a
   new board, or is only a future library option. Hardware 2.0.0 remains
   INA228-based until that decision changes the board authority.
2. Freeze the INA3221 A0 strap and exact address. INA3221 supports `0x40` through
   `0x43`; `0x41` collides with the current INA228 address.
3. Name each used channel by rail, define the enabled channel mask and current
   direction, and freeze each channel's shunt resistance and allowed range.
4. Select mode, averaging, shunt conversion time, bus conversion time, and the
   required sample coherence. Prove that the worst-case conversion fits the
   existing 1000 ms `ReadPower` deadline, or change that product contract
   deliberately.
5. Decide whether the existing scalar `vin_v` / `iin_a` sample remains mapped
   to one primary channel or whether storage, CSV, replay, cloud, CLI, web, and
   display receive a versioned three-channel contract.
6. Decide whether warning, critical, summation, and power-valid alerts are used,
   which ALERT pins are physically wired, and which owner consumes each event.
7. Keep one chip health row unless the product explicitly needs another model.
   Three INA3221 channels are not three I2C devices.

Use a static power-monitor kind in the board profile. Runtime guessing between
INA228 and INA3221 at a shared address adds failure modes and is not needed for
the first platform boundary.

### Library release gates

Use the v2 core after these focused changes:

1. Add zero-I2C binding plus cooperative initialization and reinitialization.
   A normal owner poll must execute at most its supplied transport budget.
2. Use datasheet maximum conversion timing, with a small explicit wake margin,
   whenever elapsed time is used as proof of readiness. Expose both typical and
   maximum timing for diagnostics and configuration validation.
3. Replace the competing conversion/readiness paths with one explicit staged
   job model. Add bus-silent cancellation and terminal job status.
4. Give an active job exclusive access to hardware I/O. While a job is active,
   only its poll/cancel operations and cached getters may run.
5. Make initialization, reset recovery, and reconciliation apply or verify one
   complete desired device profile, including every alert register the library
   claims to manage.
6. Route every Mask/Enable read through one decoder that retains destructive
   alert evidence before the hardware flags clear.
7. Replace the `bool` plus `Status` dirty pair with an explicit applied/unknown
   configuration state. All ambiguous controlled-register writes must make
   hardware state unknown.
8. Remove mandatory driver-owned health admission control, or provide an
   owner-managed mode that never suppresses requested I2C. `I2cTask` remains
   the health, retry, deadline, and recovery owner.
9. Add an atomic fixed-unit result with channel and quantity validity. Use a
   triggered all-enabled-channel batch when the approved product contract logs
   several rails coherently. A scalar primary-channel profile may use a smaller
   triggered result.
10. Add checked engineering-unit encoders for shunt and alert configuration.
    Reject invalid and out-of-range input instead of silently converting NaN to
    zero or clamping configuration values.
11. Tighten raw writes, status values, and the callback contract. Normal APIs
    must not write read-only registers or require message-string parsing.
12. Add the native and connected-board tests listed in this report, publish a
    new release, and pin its immutable commit in TunnelMonitor.

### Do not solve this with adapter band-aids

Avoid these long-term workarounds:

- using typical conversion delay with `pollConversionReady=false`;
- adding an owner-side alert read around a library sample;
- reconstructing the driver or calling `end()` to abandon an expired job;
- setting `offlineThreshold` to a large value;
- calling synchronous `begin()`, `recover()`, or `readBlocking()` from a normal
  `I2cTask` poll;
- assuming a sensor power-on reset occurred when only the ESP32 restarted;
- keeping alert thresholds in firmware while the library owns Mask/Enable;
- publishing partially filled staged raw fields as a successful sample;
- accepting float-only current and power in the TunnelMonitor contract;
- copying timing, sign-extension, identity, or alert-register logic into the
  adapter;
- keeping a direct and library implementation of the same selected chip/profile
  as runtime fallbacks; or
- adding a second I2C owner, generic sensor registry, plugin system, or broad
  device framework.

The missing chip behavior belongs in the reusable library. The TunnelMonitor
adapter should contain transport mapping, owner scheduling, and product policy
only.

## TunnelMonitor requirements

The library must fit the existing owner model. The firmware should not weaken
that model to fit a device driver.

| Requirement | Current authority or evidence | Consequence for INA3221 |
| --- | --- | --- |
| One I2C owner | `docs/guidelines/i2c_peripherals.md:28-35`; `docs/guidelines/target_architecture.md:306` | Only `I2cTask` may call transport. The library owns no bus, task, queue, lock, retry, scan, or bus recovery. |
| Cooperative normal work | `docs/guidelines/i2c_peripherals.md:100-133` | One normal owner poll advances at most one library transport callback. Initialization, sample, configuration, and reinitialization must be stageable. |
| Fixed timing | `include/TunnelMonitor/contracts/EnvPowerDisplay.h:50-54`; `docs/guidelines/interfaces.md:528-535` | Each callback receives at most 20 ms. `ReadPower` retains one 1000 ms deadline from original admission; library phases do not renew it. |
| Current board facts | `include/TunnelMonitor/BoardPins.h:26-30,80-87`; `include/TunnelMonitor/i2c/I2cConfig.h:9` | Hardware 2.0.0 uses GPIO8/GPIO9 at 400 kHz and specifies INA228 at `0x41`. No INA3221 address or ALERT pin is currently authoritative. |
| Current electrical profile | `include/TunnelMonitor/contracts/EnvPowerDisplay.h:60-63` | The existing power path has one 25,000 micro-ohm shunt, 1500 mA warning, 2500 mA maximum, and 15000 mV bus maximum. These values cannot be silently applied to three INA3221 channels. |
| Current scalar result | `include/TunnelMonitor/contracts/EnvPowerDisplay.h:135-145`; `src/measurement/MeasurementAssembler.cpp:220-251` | The result has one bus voltage, shunt voltage, current, power, and INA228 die temperature. INA3221 has three channels and no die-temperature measurement. |
| Current sample schema | `docs/guidelines/measurement_data.md`; `src/measurement/MeasurementAssembler.cpp` | The durable sample exposes scalar input voltage/current. Adding three rails requires an explicit schema and serialization decision even if fixed record capacity remains available. |
| Optional-device health | `docs/guidelines/time_health_watchdog.md:243-262` | Missing power monitoring is optional/disabled, not aggregate system failure. Hotplug and later reappearance must work without reboot. |
| Owner cadence and stale policy | `docs/guidelines/i2c_peripherals.md:416-423` | Firmware owns the 5000 ms background cadence, 15000 ms stale rule, last-good status, and publication. The chip library does not. |
| Fixed memory | `include/TunnelMonitor/i2c/I2cTask.h:121-149` | No heap growth, dynamic containers, unbounded retry, or unbounded wait in steady operation. |
| Private dependency boundary | `docs/guidelines/ownership.md` and `docs/guidelines/dependency_policy.md` | INA3221 types stay behind an owner-private adapter. Public contracts remain project-owned and third-party-free. |

The current firmware implements INA228 identity, calibration, diagnostics, and
five measurement reads directly in `src/i2c/I2cTask.cpp:2622-2835`. INA3221
integration should introduce a narrow statically selected power-driver boundary
for its approved board profile, not another set of protocol branches in that
state machine. The current INA228 path may remain for hardware 2.0.0. It should
be refactored behind its own adapter separately if that board stays supported.

## Fit matrix

| Area | INA3221 v2.0.0 state | TunnelMonitor decision |
| --- | --- | --- |
| Framework-neutral injected transport | Good | Keep |
| Fixed storage and no steady-path heap | Good | Keep |
| Identity, register endian, signed measurement decode | Good | Keep |
| Typed mode, conversion-time, averaging, and channel enums | Good | Keep and tighten masks/address |
| Precise underlying transport status | Mostly good | Keep; split overloaded state errors |
| Instruction-budgeted sample job | Partial | Keep engine; add cancellation, exclusivity, and terminal result |
| Zero-I2C bind | Missing | Required |
| Cooperative initialization/reinitialization | Missing | Required |
| Maximum-time readiness | Incorrect | Required correctness fix |
| Complete desired device profile | Missing | Required if configuration/alerts are managed |
| Alert event retention | Missing | Required when Mask/Enable is read |
| Configuration uncertainty tracking | Partial | Replace with explicit applied state |
| Atomic coherent fixed-unit result | Missing | Required for selected channel(s); full three-channel batch only if the product contract needs it |
| External health ownership | Conflicts with mandatory library `OFFLINE` | Required refactor |
| Owner deadline cancellation | Missing | Required |
| Exact TunnelMonitor board definition | Missing by product design | Product decision required |
| Exact TunnelMonitor native and HIL evidence | Missing | Required before dependency selection |

## What already fits

These v2.0.0 properties should be preserved:

- Core headers and source do not depend on Arduino, ESP-IDF, FreeRTOS, or
  `Wire`.
- Write and write-read transport, time, and optional yield are injected through
  non-owning callbacks (`include/INA3221/Config.h:11-49`).
- The core uses fixed arrays and contains no dynamic container or deliberate
  heap allocation in normal paths.
- `Channel`, `Mode`, `ConvTime`, and `Averaging` encode real device fields.
- Manufacturer ID `0x5449` and die ID `0x3220` are verified by `probe()`.
- Shunt and bus values are sign-extended from the INA3221 13-bit register
  formats. Bus voltage has the documented 8 mV LSB even though a negative bus
  result is outside TunnelMonitor's valid board operating range.
- Current and power are calculated on the host because INA3221 does not contain
  current or power registers.
- The staged sample engine accepts an instruction budget and can perform one
  transport callback per `pollJob()` call.
- Disabled-channel and mode guards exist.
- Transport failures are not collapsed into a generic probe failure in v2.
- Configuration-write uncertainty is recognized for the Configuration and
  Mask/Enable registers, even though the model is incomplete.
- Read-clear behavior is documented in several public comments.
- The repository has useful native tests, Arduino examples, command-line
  contract checks, package checks, and retained HIL summary evidence.

The v2 staged engine is the correct refactor base. A new scheduler or generic
driver framework is not needed.

## Hard findings

### H-01: INA3221 is not part of the current TunnelMonitor board contract

Priority: product integration blocker

TunnelMonitor hardware 2.0.0 fixes an INA228 at `0x41`
(`include/TunnelMonitor/BoardPins.h:80-87`). `PowerReadResult` is scalar and
contains INA228 die temperature (`include/TunnelMonitor/contracts/EnvPowerDisplay.h:135-145`).
The durable measurement path also consumes one input voltage and one input
current. INA3221 provides three bus/shunt channels and no die-temperature
measurement.

INA3221 addresses are `0x40`, `0x41`, `0x42`, and `0x43`. Using `0x41` while
the current INA228 is populated is an electrical address collision, not a
software selection problem.

The current I2C known-device table has exactly five entries and one unique
`PowerMonitor` row (`include/TunnelMonitor/i2c/I2cConfig.h:81`;
`src/i2c/I2cDiagnostics.cpp:45-54,1025-1040`). Production health capacity is
already exactly 16 devices (`include/TunnelMonitor/contracts/Capacities.h:81-90`;
`docs/guidelines/time_health_watchdog.md:271-280`). Replacing the monitor under
the existing `DeviceId::PowerMonitor` can preserve that shape. INA228 and
INA3221 coexistence is not one extra array row; it needs a deliberate identity,
status, diagnostics, and health-capacity design.

Required action:

- Define a board profile that statically selects INA3221 and its A0 strap.
- Freeze channel names, shunts, direction, limits, and enabled mask.
- Choose scalar compatibility or a versioned three-channel public result.
- If coexistence is required, approve the device-identity and fixed-capacity
  changes rather than adding three channel health rows.
- Do not modify current BoardPins or hardware 2.0.0 under this audit.

### H-02: delay-only readiness uses typical conversion time

Priority: measurement correctness blocker

`convTimeUs()` uses `140, 204, 332, 588, 1100, 2116, 4156, 8244` microseconds
(`src/INA3221.cpp:147-163`). These are typical conversion times.
`getCycleTimeUs()` multiplies them by shunt plus bus conversions, enabled
channels, and averaging (`src/INA3221.cpp:1396-1418`). When readiness polling is
disabled, `_pollSampleJob()` starts reading as soon as that calculated delay
expires (`src/INA3221.cpp:1735-1747`).

The datasheet maximum conversion times are `154, 224, 365, 646, 1210, 2328,
4572, 9068` microseconds. At three channels, shunt plus bus, and 1024 averages,
the library calculates 50.651136 seconds while the datasheet maximum is
55.713792 seconds. A delay-only job may read about 5.063 seconds early.

This is also a product configuration problem: either value is far above
TunnelMonitor's current 1000 ms operation deadline.

Required refactor:

- Store both typical and maximum time for every `ConvTime`.
- Calculate cycle time with checked 64-bit arithmetic.
- Use maximum time plus a named wake/scheduling margin for delay-only readiness.
- Let `CVRF` confirm readiness when destructive Mask/Enable semantics are
  acceptable.
- Reject a selected profile that cannot finish inside the owner deadline.

### H-03: initialization does not establish a complete known device profile

Priority: configuration correctness blocker

`begin()` clears live state, probes two identity registers, and writes the
Configuration register (`src/INA3221.cpp:228-305`). It does not reset the chip,
read existing Mask/Enable state, or apply alert limits, summation limit, or the
power-valid window. `_maskEnableWritableCache` starts at zero even if the
sensor retained different settings across an ESP32-only restart.

It also clears the current live state before the candidate configuration is
fully validated. Calling `begin()` again with an invalid configuration can
discard a previously usable binding even though no replacement was accepted.

The first later summation or latch setter builds a whole Mask/Enable write from
that assumed-zero cache (`src/INA3221.cpp:1284-1320`). It can unintentionally
overwrite retained hardware settings. A successful `begin()` therefore does
not mean the complete device state is known.

Required refactor:

- Separate zero-I2C `bind()` from a staged initialization job.
- Validate a candidate profile completely before replacing the current bound
  profile.
- Define one complete desired `DeviceProfile`.
- During initialization, either issue a staged software reset and apply the
  complete profile, or read and reconcile every modeled register.
- Read back critical configuration before publishing `Applied` state.
- Do not assume sensor POR from MCU boot.

### H-04: recovery cannot restore the configuration the library exposes

Priority: field recovery blocker

`recover()` performs identity reads and reapplies only Configuration and cached
Mask/Enable (`src/INA3221.cpp:375-425`). Per-channel critical/warning limits,
shunt-sum limit, and power-valid limits are written directly by separate APIs
and are not retained in a complete desired profile
(`src/INA3221.cpp:1129-1243`).

After sensor reset, brownout, or replacement, `recover()` can return success
while those registers remain at reset defaults. This is unsafe if alerts are
used for protection or field diagnostics.

Required refactor:

- Put every managed volatile register in `DeviceProfile`/`AlertProfile`.
- Reuse the same cooperative apply-and-verify job for initialization,
  reappearance, reset, and explicit configuration change.
- If the first TunnelMonitor integration does not use chip alerts, omit those
  APIs from its profile deliberately; do not pretend they were restored.

### H-05: Mask/Enable reads clear alert evidence that the library discards

Priority: diagnostics correctness blocker when alerts are enabled

Reading INA3221 Mask/Enable clears the critical, warning, summation, and
conversion-ready flags; power-valid is condition-level and timing-control is
sticky until power/reset. The datasheet documents these semantics on PDF pages
34 and 35.

The staged readiness path reads Mask/Enable and inspects only `CVRF`
(`src/INA3221.cpp:1752-1765`). `tickStatus()` and `readConversionReady()` can do
the same through the shared helper (`src/INA3221.cpp:1896-1937`). Any alert bits
present in the same read are lost. Existing tests explicitly model flag
clearing (`test/test_basic.cpp:923-953,1363-1404`), so this is not hypothetical.

Required refactor:

- Route every Mask/Enable read through one typed decoder.
- OR-latch destructive events into fixed software state before returning.
- Provide `peekAlertEvents()` and `takeAlertEvents()` or equivalent clear
  ownership.
- Include the raw/decoded alert snapshot in the sample that consumed `CVRF`.
- Name direct reads as destructive; `readAlertFlags()` is not a non-mutating
  read.

### H-06: staged jobs have no safe deadline cancellation

Priority: liveness blocker

The public staged API provides start, poll, raw-step, and snapshot operations,
but no cancel operation (`include/INA3221/INA3221.h:196-231`). If `CVRF` never
sets, the owner can receive `IN_PROGRESS` indefinitely. A transport error from
`_pollSampleJob()` returns without creating a clean terminal result or clearing
the active job (`src/INA3221.cpp:1691-1806`). A later start then returns `BUSY`.

The snapshot exposes progressively filled raw channel fields while the job is
still active (`include/INA3221/INA3221.h:65-88`; `src/INA3221.cpp:779-796`).
Each channel does have `channelEnabled`, `shuntValid`, and `busValid`, and the
snapshot has `complete`. The actual gap is that partial work is not separated
from an atomically committed completed batch or a last-good sample. An adapter
could still publish partial data after a deadline or mid-sample error.

Required refactor:

- Add `cancelJob()` that performs no I2C and discards partial sample data.
- Keep the last completed sample unchanged until a new batch commits atomically.
- Retain terminal status separately from active progress.
- Report whether cancellation left hardware configuration uncertain, for
  example after an ambiguous trigger/configuration write.
- Test cancellation from every stage and immediately after every I/O error.

### H-07: two state paths can interfere and active jobs lack exclusivity

Priority: state-machine blocker

The older `startConversion()` / `tickStatus()` state and the newer
`startSingleShot()` / `pollJob()` state share conversion-ready state but are not
one operation (`src/INA3221.cpp:313-322,630-740,1691-1806`). `tickStatus()` may
read and clear `CVRF` independently while a sample job is waiting for it.

Synchronous measurement, reset, configuration, raw-register, and alert APIs do
not consistently reject an active staged job. Some configuration side effects
silently clear poll-job state. The result can be a stuck job, mixed settings,
or a sample assembled across a configuration change.

Required refactor:

- Keep one job engine for init, profile apply, triggered sample, and explicit
  reconcile/reset work.
- Reject every hardware I/O API with a distinct `JOB_BUSY` while a job is
  active, except that job's poll/cancel methods.
- Permit only clearly named cached getters during an active job.
- Move blocking/readiness convenience to a wrapper that drives the same job
  engine; it must not maintain parallel state.

### H-08: lifecycle and blocking calls hide too much work

Priority: architecture blocker

`begin()` performs two identity reads and a configuration write in one call.
`recover()` performs two identity reads and two writes. `end()` attempts a
power-down write and discards its result. `readBlocking()` loops until a
deadline and can issue repeated Mask/Enable reads
(`src/INA3221.cpp:228-425,798-858`).

These APIs are reasonable standalone conveniences but do not fit the normal
TunnelMonitor rule of one transport callback per owner poll. A hidden
power-down write is also the wrong behavior for a zero-I2C unbind/destructor
path.

Required refactor:

- Make bind/unbind/destruction zero-I2C.
- Provide explicit staged initialize, apply, sample, power-down, and
  reinitialize operations.
- Keep blocking helpers only as optional convenience wrappers and exclude them
  from the TunnelMonitor adapter.
- Return every explicit power-down result; do not discard it.

### H-09: driver-owned offline policy conflicts with `I2cTask`

Priority: ownership blocker

Every transfer updates driver counters. After a configured number of failures,
the driver latches `OFFLINE` and blocks normal I/O until library `recover()` is
called (`src/INA3221.cpp:1425-1472,1541-1609`). `recover()` then performs the
multi-transfer configuration policy described above.

TunnelMonitor already owns optional presence, error counters, stale status,
retry/backoff, deadline expiry, and shared-bus recovery. Two health authorities
can disagree and can prevent the owner from issuing the transaction it needs
to diagnose a reappeared device.

Required refactor:

- Make the reusable core a passive protocol component.
- Expose transfer counters and last status only as passive diagnostics, if they
  remain useful.
- Never suppress an owner-requested transfer because of library health state.
- Let `I2cTask` decide when to start the bounded reinitialize job.

### H-10: configuration uncertainty is incomplete

Priority: data trust blocker

Failed Configuration and Mask/Enable writes can mark
`hardwareConfigDirty()`, but failed alert-limit writes do not
(`src/INA3221.cpp:1129-1243,1613-1623`). Those writes can also time out after the
device accepted them, so the hardware state is ambiguous.

A successful raw write to Configuration or Mask/Enable marks the bool dirty
using a `Status` whose error value is `OK` (`src/INA3221.cpp:1488-1504`). The
caller can therefore see `hardwareConfigDirty() == true` while
`hardwareConfigDirtyStatus().ok() == true`. Measurements are not blocked or
quality-marked when configuration is unknown.

Required refactor:

- Replace the pair with `AppliedConfigState { Unknown, Applied, Dirty }` plus a
  separate last transport status.
- Treat timeout/unknown-result writes to every controlled register family as
  `Unknown`.
- Do not publish engineering-unit data while shunt calibration, enabled
  channels, mode, or timing may differ from the desired profile.
- Track measurement-affecting and alert-only uncertainty separately. Unknown
  alert thresholds invalidate alert guarantees, but do not by themselves make
  an otherwise verified voltage/current sample invalid.
- Reconcile by readback or a complete apply job. Do not blindly repeat an
  ambiguous write.

### H-11: current sample APIs do not provide a coherent platform result

Priority: integration correctness blocker

`ChannelMeasurement` contains only float bus voltage, shunt millivolts,
current milliamps, and power milliwatts. It has no channel/quantity validity,
capture time, configuration generation, or alert evidence
(`include/INA3221/INA3221.h:23-29`). `readPower()` and `readChannel()` perform
separate register reads (`src/INA3221.cpp:539-588`). The staged continuous read
also reads channels sequentially (`src/INA3221.cpp:1769-1792`).

INA3221 updates channel registers sequentially in continuous mode. A
multi-register read can cross an ADC update boundary and combine different
cycles. The existing fake registers do not change during a read, so current
tests do not prove coherence.

Required refactor:

- Make the TunnelMonitor path a triggered sample: write the triggered profile,
  wait, capture Mask/Enable/`CVRF`, then read the product-selected channel(s)
  after the device returns to power-down. Read all enabled channels only when
  the approved contract needs a coherent multi-rail batch.
- Build a private work buffer and commit one complete `SampleBatch` only after
  every field required by the selected result succeeds.
- Include enabled, valid-channel, and valid-quantity masks, capture uptime,
  profile generation, and alert snapshot.
- Keep continuous reads available only with explicit mixed-age semantics, or
  add and prove a stronger coherence method before using them for logging.

### H-12: unit/configuration helpers are too permissive

Priority: safe-configuration blocker

Alert setters primarily accept raw register-format `int16_t` values
(`include/INA3221/INA3221.h:276-329`). Public float encoders silently convert
non-finite values to zero and clamp out-of-range input
(`src/INA3221.cpp:132-145,1388-1394`). `getShuntResistance()` returns `0.0` for
an invalid channel (`src/INA3221.cpp:1029-1040`).

Silent clamp is convenient for drawing or display values. It is unsafe for a
protection threshold or calibration because a caller cannot distinguish its
request from the value actually applied.

`Config` also defaults all three shunts to 0.1 ohm
(`include/INA3221/Config.h:116-117`). A caller that forgets to override a real
board shunt receives plausible but wrong current and power. The hardware sum
register adds shunt voltages; it represents summed current only when the
selected shunts have equal resistance.

Required refactor:

- Add checked fixed-unit encoders/decoders for shunt microvolts, bus
  millivolts, threshold registers, and the power-valid window.
- Require an explicit valid shunt calibration for every enabled channel.
- Reject NaN, infinity, invalid channel, and out-of-range values.
- Validate that lower power-valid limit is below upper limit.
- Use 64-bit intermediates and define rounding for current and power.
- Keep raw helpers explicitly named and limited to diagnostics/tests.

### H-13: raw writes, timeout context, and state errors need tightening

Priority: maintainability and fault-diagnosis blocker

`writeRegister16()` accepts the general valid-register set, which includes
read-only identity and measurement registers (`src/INA3221.cpp:62-70,1488-1505`).
Raw writes can bypass typed side effects and desynchronize cached state.

The public driver also leaves compiler-generated copy and move operations
available. Copying a configured driver duplicates its callback/user pointers,
health state, cached configuration, and any live job state. Two copies can then
operate the same physical device with divergent internal state.

Every operation also uses one `Config::i2cTimeoutMs` value. Instruction budgets
limit callback count, not elapsed wall time (`include/INA3221/Config.h:90-120`).
The owner needs to clamp each attempt to both its 20 ms transfer cap and the
remaining operation deadline. The callback comment does not fully define the
chosen transaction shape, exact-length result, or whether hidden retry is
permitted (`include/INA3221/Config.h:21-32`). The chip allows the register
pointer write to end with STOP and retain the pointer. TunnelMonitor may still
choose one non-interleaved transmit-receive/repeated-START backend operation as
owner policy; it is not a universal INA3221 protocol requirement.

Finally, `BUSY` represents several different conditions, including offline,
active conversion, wrong mode, and competing job. Production code must not
parse `Status::msg` to tell them apart.

Required refactor:

- Restrict normal raw writes to documented writable registers and make the
  diagnostic escape hatch clearly unsafe/private.
- Delete copy and move operations unless explicit shared-device copy semantics
  are designed and tested.
- Pass a per-poll timeout or deadline context so the owner can supply remaining
  time without mutating global configuration.
- State exact callback semantics: one physical attempt, exact lengths, the
  selected pointer-write/read transaction policy, no owner interleaving, and no
  hidden retry or bus recovery.
- Add distinct status codes such as `JOB_BUSY`, `DEVICE_OFFLINE`,
  `CONVERSION_BUSY`, `CONVERSION_NOT_READY`, and `CONFIG_UNKNOWN`.

### H-14: release validation is useful but not sufficient for this integration

Priority: release gate

The maintained release builds and tests well, but its fakes and CI do not prove
the behavior TunnelMonitor depends on. The fake bus ignores the I2C address and
timeout arguments, accepts loose lengths, and does not model the selected
pointer-write/read transaction shape, short transfer, register changes during
sequential reads, or a write that was committed before timeout. No test
exercises a permanently low `CVRF`, public cancellation, negative live
shunt/current/power, destructive alert retention, or full profile recovery.

The CI uses unpinned `platform = espressif32`, does not compile the advertised
pure ESP-IDF component/example, and does not run HIL. TunnelMonitor itself uses
its pinned pioarduino/Arduino framework and an ESP-IDF 5 I2C backend; it does
not consume the pure-IDF component. These are normal library portability gaps,
while the required TunnelMonitor gate is its exact pinned ESP32-S3 build with
the private adapter.

Required action:

- Add the targeted tests in the validation section below.
- Exact-pin library CI toolchains used to qualify a release.
- Compile the pure ESP-IDF component if the library release continues to claim
  that portability; do not describe it as TunnelMonitor's production path.
- Build the exact pinned TunnelMonitor ESP32-S3 Arduino firmware with the
  private adapter and ESP-IDF I2C backend.
- Run exact-board, shared-bus HIL and retain the build identity and condensed
  evidence.

## Recommended library shape

Keep the library chip-specific. The following is a focused API shape, not a
generic sensor framework.

### Configuration types

- `enum class Address : uint8_t { A0Gnd = 0x40, A0Vs = 0x41, A0Sda = 0x42,
  A0Scl = 0x43 }`
- Existing `Channel`, `Mode`, `ConvTime`, and `Averaging`, with stable explicit
  underlying values where persisted or logged.
- `ChannelMask`, with `channelBit(Channel)`, `contains()`, `count()`, and
  validation that no bits outside channels 1 through 3 are set.
- `ShuntCalibration { uint32_t resistanceMicroOhms; CurrentDirection direction;
  }` for each channel. Add gain/offset only after a real calibration requirement
  exists.
- `ConversionTiming { uint32_t typicalUs; uint32_t maximumUs; }` and a checked
  `maximumCycleTimeUs(profile)` helper.
- `AlertProfile` containing per-channel critical/warning limits, summation
  channel mask and limit, latch settings, and validated power-valid lower and
  upper values.
- `DeviceProfile` containing address, enabled channels, conversion settings,
  per-channel shunt calibration, and `AlertProfile`.
- `enum class AppliedConfigState : uint8_t { Unknown, Applied, Dirty }`, tracked
  separately for measurement settings and alert-only settings.

`DeviceProfile` must be fixed-size, trivially owned, and independent of Arduino
or ESP-IDF objects. If alerts are not supported in the first release, remove
`AlertProfile` rather than leaving it partially applied.

Require every used channel's shunt resistance explicitly; do not rely on the
library's plausible-looking 0.1-ohm defaults
(`include/INA3221/Config.h:116-117`). The INA3221 summation register adds shunt
voltages, not calibrated channel currents. If summation is interpreted as total
current, all selected shunts must have the same resistance. Otherwise keep the
result as a voltage sum and name it that way.

### Sample and event types

- `RawChannelSample { uint16_t shuntRegister; uint16_t busRegister; }`; pure
  decoders produce signed 13-bit codes or fixed units.
- `FixedChannelReading { int32_t busMilliVolts; int32_t shuntMicroVolts;
  int32_t currentMilliAmps; int32_t powerMilliWatts; QuantityMask valid; }`.
- `AlertEventMask` plus decoded per-channel critical/warning fields, summation,
  power-valid, timing-control, and conversion-ready evidence.
- `SampleBatch { FixedChannelReading channels[3]; ChannelMask enabled;
  ChannelMask validChannels; AlertSnapshot alerts; uint64_t captureUptimeMs;
  uint32_t profileGeneration; }`.
- `JobSnapshot` containing job kind, stage, terminal/in-progress state,
  instructions used in the last poll, and terminal `Status`. Do not expose
  partially filled publishable samples.

Use exact device units where possible:

- shunt voltage is signed and has a 40 microvolt LSB;
- bus voltage is signed two's-complement and has an 8 millivolt LSB;
- current in milliamps is computed with a 64-bit intermediate from shunt
  microvolts and shunt micro-ohms;
- power in milliwatts uses a 64-bit intermediate and preserves negative power
  when reverse current is allowed.

The INA3221 signed bus register can encode approximately -32.768 V through
+32.760 V, but the datasheet bus input operating range is 0 V through 26 V.
Keep register decoding range separate from board electrical limits. A negative
decoded bus result must be retained for diagnostics and rejected as invalid for
TunnelMonitor's normal board reading; do not clamp it to zero.

### Cooperative operations

The core needs one operation model:

1. `bind(profile, transport)` validates and stores configuration without I2C.
2. `startInitialize()` / `startApplyProfile()` / `startTriggeredSample()` start
   one explicit job without hidden loops.
3. `pollJob(nowMs, instructionBudget, transferTimeoutMs)` advances no more than
   the budget and returns progress plus instructions used.
4. `cancelJob()` performs no I2C, discards partial results, and reports whether
   hardware state is uncertain.
5. `takeCompletedSample()` returns only an atomically completed batch.
6. `unbind()` performs no I2C. Power-down is a separate explicit job.

An initialize/apply job should perform identity, desired-register writes, and
selected readback in bounded stages. A triggered sample should trigger once,
wait from the owner's clock, consume one Mask/Enable snapshot, and then read the
channel registers required by the approved result. Each normal TunnelMonitor
poll supplies a budget of one callback.

### Alert ownership

Mask/Enable has destructive read semantics, so one code path must own every
read. The decoder should:

- return the complete raw value to the current operation;
- update condition-level state such as power-valid;
- OR-latch critical, warning, summation, and conversion-ready events;
- retain timing-control until an explicit reset/rebind observation says it
  cleared; and
- expose separate peek and take/acknowledge operations.

Do not split alert ownership between the sample job, `tickStatus()`, and an
owner-side workaround.

## TunnelMonitor integration boundary

### Keep in the INA3221 library

- register addresses and endian handling;
- configuration bit encoding;
- ID verification;
- signed shunt and alert-limit decoding;
- maximum conversion timing;
- destructive Mask/Enable decoding and event retention;
- fixed-unit conversion using supplied shunt calibration;
- the cooperative chip job state machine; and
- configuration apply/readback semantics.

### Keep in `I2cTask`

- ESP-IDF backend adaptation and the selected non-interleaved register-read
  transaction mapping;
- the single I2C queue, arbitration, one-callback poll budget, and 20 ms cap;
- the original 1000 ms command deadline and cancellation at expiry;
- optional presence, health, stale state, retry/backoff, and bus recovery;
- board-profile selection, address/A0 strap, wired enabled channels, physical
  shunt values, and electrical direction;
- command/result publication; and
- mapping library status into TunnelMonitor error/health vocabulary.

### Keep in application policy

- rail names and which channel is primary;
- accepted operating limits and warning/health policy;
- warning/over-range classification;
- storage/schema versioning;
- display, web, CLI, and cloud presentation; and
- whether a chip alert changes health or is only diagnostic.

The adapter should be a small private member of the existing I2C owner. It
should not expose INA3221 headers through `include/TunnelMonitor/contracts/`.

## Suggested TunnelMonitor contract direction

No public contract change should be made until the product decisions are
accepted. If the platform needs a reusable multi-channel result, prefer one
fixed-capacity project-owned shape similar to:

```cpp
enum class PowerMonitorKind : uint8_t {
  Unknown = 0,
  Ina228 = 1,
  Ina3221 = 2,
};

enum class PowerChannelId : uint8_t {
  Channel1 = 1,
  Channel2 = 2,
  Channel3 = 3,
};

struct PowerChannelReading {
  int32_t busMilliVolts;
  int32_t shuntMicroVolts;
  int32_t currentMilliAmps;
  int32_t powerMilliWatts;
  uint16_t validFlags;
  uint16_t alertFlags;
};

struct PowerReadResult {
  PowerMonitorKind kind;
  uint8_t address;
  uint8_t channelCount;
  uint8_t enabledMask;
  uint32_t deviceFlags;
  int32_t dieTemperatureMilliCelsius;
  uint32_t validFlags;
  PowerChannelReading channels[3];
  uint64_t readUptimeMs;
};
```

This is a direction, not an approved ABI. `Unknown` describes monitor identity;
existing device-health state continues to describe absent/disabled and should
not be duplicated as a `None` kind. Chip-wide fields stay outside the channel
array. INA228 may set die-temperature validity; INA3221 must leave that validity
clear rather than synthesizing zero. Give channel fields explicit validity and
use a fixed array. A negative INA3221 bus decode is diagnostic but invalid for
the board operating range; preserve it and clear bus validity instead of
clamping it. Static-assert the final contract below the existing 128-byte I2C
payload/result bound.

If the product only needs one INA3221 rail, the simpler safe choice is to map
one statically selected primary channel into the existing scalar contract and
keep the other channels unused. Do not expose three channels only through
ad-hoc CLI or status side paths.

## Helpers worth adding

### Required helpers

- `channelBit(Channel)`, `channelIndex(Channel)`, `contains(ChannelMask,
  Channel)`, and `enabledChannelCount(ChannelMask)` with checked invalid input;
- `conversionTiming(ConvTime)` and `maximumCycleTimeUs(DeviceProfile)`;
- `decodeShuntMicrovolts(uint16_t)` with explicit 13-bit sign extension;
- `decodeBusMillivolts(uint16_t)` with explicit 13-bit sign extension;
- checked encode/decode helpers for critical, warning, summation, and
  power-valid limits;
- `validatePowerValidWindow(lower, upper)`;
- `decodeMaskEnable(uint16_t)` used by every read of that register;
- `isReadableRegister()` and `isWritableRegister()` instead of one broad
  register predicate; and
- pure `convertSample(raw, calibration, out)` that can be exhaustively tested
  without a bus.

### Nice-to-have helpers

- `addressFromA0(A0Connection)` and a typed address-to-byte conversion;
- `expectedTransactionCount(DeviceProfile, JobKind)` for diagnostics/tests;
- `toString()` for stable enum/status diagnostics without heap allocation;
- a decoded `ConfigurationRegister` and `MaskEnableSnapshot` for log output;
- `sampleAgeMs(now, sample)` using the project/owner's wrap-safe time policy;
- compile-time size/trivial-copy assertions for public fixed records; and
- a host-only profile printer for tests and HIL setup.

Do not add dynamic maps, strings, per-channel objects allocated on the heap, or
a generic device registry to provide these helpers.

## Validation performed

Validation was run against exact INA3221 v2.0.0 at `bcf1abb` in a temporary
detached worktree. Generated artifacts and that worktree were removed after the
audit.

| Check | Result | Notes |
| --- | --- | --- |
| Native Unity suite | PASS | 81 of 81 tests passed |
| Core timing guard | PASS | Repository timing guard script passed |
| CLI contract | PASS | Contract script passed |
| ESP-IDF example contract script | PASS | Static contract check passed; this is not a compiled IDF build |
| Version consistency | PASS | `library.json`, `idf_component.yml`, and `Version.h` report 2.0.0 |
| Arduino ESP32-S3 build | PASS | 41,768 bytes RAM; 433,222 bytes flash in the local resolved environment |
| Arduino ESP32-S2 build | PASS | 37,024 bytes RAM; 384,973 bytes flash in the local resolved environment |
| Normal host warning build | PASS | `-Wall -Wextra -Wpedantic -Werror` |
| Strict conversion warning build | FAIL | Five int/sign conversion warnings at `src/INA3221.cpp:186,191,196,201,1310`; not enforced by current CI |
| Doxygen | PASS | Doxygen 1.15 exited zero with no warnings printed; `WARN_AS_ERROR` is currently disabled |
| Package creation | PASS | `pio pkg pack` completed successfully; archive size was not treated as release evidence because the audit used a linked worktree |
| HIL parser self-test | PASS | Runner parser self-test passed |
| Pure ESP-IDF compile | NOT RUN | `idf.py` was not installed in the audit environment |
| UBSan | NOT RUN | Local MinGW toolchain lacked the UBSan runtime library |

The locally resolved Arduino platform was Espressif32 54.3.20 / Arduino 3.2.0.
The library configuration does not exact-pin that platform, so these build
sizes are evidence for this audit environment, not a reproducible production
toolchain statement.

## Existing HIL evidence

`docs/reports/hil-validation-summary-20260701.md` records a final ESP32-S3
Arduino/TinyUSB run with:

- 69 main-suite passes and no failures;
- a two-hour soak with 13,542 successful commands and no failures;
- 846 recovery commands and no failures; and
- 47 ms reported worst command time.

This is useful evidence that the basic driver and runner operated for an
extended healthy-bus test. It is not exact TunnelMonitor qualification:

- the run started from an earlier commit with dirty build/runner files;
- later v2 changes did not change `src/`, `include/`, or `test/`, but no firmware
  artifact hash was retained;
- raw transcripts were intentionally discarded;
- electrical disconnect, stuck-line, and unsafe-stimulus fault injection were
  omitted;
- alert tests configured/inspected outputs but did not induce physical
  thresholds or verify ALERT pins;
- negative shunt/current was not tested through the live device path;
- only the library's ESP32-S3 Arduino runner was exercised, not the pinned
  TunnelMonitor firmware/private adapter; the separate pure-IDF portability
  path was also not compiled; and
- fixture wiring and load plausibility remained manual checks; no
  calibrated-reference evidence was retained.

Do not claim this as INA3221-on-TunnelMonitor HIL.

## Required native tests before release

Add focused tests for:

1. all conversion-time, averaging, and enabled-channel combinations using
   datasheet maximum times and checked arithmetic;
2. delay-only readiness never occurring before the maximum deadline;
3. permanently low `CVRF`, owner deadline expiry, and bus-silent cancellation;
4. cancellation from every job stage and after every transport failure;
5. active-job exclusion against tick, reset, raw access, alert access, and all
   setters;
6. alert-event preservation through every Mask/Enable consumer;
7. warning, critical, summation, power-valid, timing-control, and latch decode;
8. MCU restart while a powered sensor retains non-default registers;
9. complete profile restoration after sensor reset/power loss;
10. commit-then-timeout ambiguity for Configuration, Mask/Enable, and every
    managed alert-limit register;
11. configuration readback mismatch and `AppliedConfigState` transitions;
12. negative one-LSB, negative full-scale, positive full-scale, and saturation
    for live shunt, shunt sum, current, power, and alert getters, plus signed bus
    decode boundaries and rejection outside the 0 V to 26 V operating range;
13. NaN, infinity, range, rounding, and lower/upper-window validation;
14. exact address and timeout propagation through the fake transport;
15. exact tx/rx lengths, the selected pointer-write/read transaction shape,
    no owner interleaving, short transfer, NACK, and bus/timeout status
    preservation;
16. register mutation between continuous reads to prove or reject coherence;
17. atomic sample commit and last-good preservation after each mid-sample
    failure; and
18. 32-bit clock rollover plus one consistent clock domain between start and
    poll.

The fake bus should reject the wrong address, wrong timeout, wrong length, and
wrong transaction shape. Otherwise the tests can pass while the production
adapter violates the transport contract.

## Required TunnelMonitor native tests

After a new library release is exact-pinned, add owner-level tests for:

- the approved board profile and exact address;
- address collision rejection where INA228 and INA3221 would both be `0x41`;
- one callback per normal `I2cTask` poll;
- the 20 ms per-transfer cap and original 1000 ms operation deadline;
- cancellation at deadline without later hidden I2C;
- exact mapping of backend NACK, timeout, short transfer, and bus errors;
- optional absence, reappearance, stale status, and owner-controlled recovery;
- no library `OFFLINE` state blocking owner attempts;
- complete validity for the approved scalar or multi-channel result and no
  partial-success publication;
- scalar-primary or versioned multi-channel contract mapping, as approved;
- INA3221 die-temperature validity remaining clear;
- alert flags surviving the sample's destructive readiness read;
- fixed payload/result size and no heap growth; and
- no duplicate direct/library implementation for the same selected
  chip/profile after adapter qualification.

## Required connected-board HIL

Run HIL on the actual ESP32-S3 board revision and approved INA3221 assembly:

1. Record board revision, INA3221 ordering code, A0 strap/address, shunt values,
   firmware commit, library commit, build artifact hash, toolchain, bus speed,
   and wiring.
2. Verify all enabled channels at zero and at known loads. Cover positive and,
   if electrically supported, reverse current.
3. Compare bus voltage, shunt voltage, current, and calculated power against
   calibrated references with written tolerances.
4. Exercise the selected worst-case timing profile and prove the whole command
   stays inside its owner deadline.
5. Stress the shared 400 kHz bus with OLED, FRAM, RTC, and environmental sensor
   activity while sampling all enabled channels.
6. Disconnect/reconnect the INA3221 and power-cycle only the sensor while the
   ESP32 remains running. Verify complete profile restoration.
7. With a controlled safe fault fixture, inject NACK, disconnect/reconnect, and
   stuck SDA/SCL cases. Verify that only `I2cTask` performs bus recovery and
   that unrelated I2C devices continue.
8. If alerts are wired, cross each configured warning/critical/summation and
   power-valid threshold and verify pin behavior plus software event retention.
9. If a programmable target/fault fixture can produce commit-then-timeout or
   exact short-read cases safely, verify them physically. Otherwise retain the
   required native evidence and record these HIL cases as not run.
10. Run a multi-hour soak with field-rate sampling, retain condensed evidence,
    and report worst operation time, error counts, and recovery counts.

Physical ALERT behavior is not a release gate if the approved first profile
does not wire or use those pins. In that case, state the omission and keep the
unused alert policy disabled in the complete profile.

## Recommended implementation sequence

1. Resolve the product and board-profile decisions. Do not change hardware
   2.0.0 facts implicitly.
2. Keep library work based on INA3221 v2.0.0 or its focused successor, not the
   old v1.2 audit branch.
3. Refactor timing, complete profile ownership, alert retention, one job engine,
   cancellation, exclusivity, and external health ownership.
4. Add fixed-unit atomic result conversion and checked helpers.
5. Add the targeted library tests. Compile the pure ESP-IDF component if the
   library continues to advertise it.
6. Publish a reviewed immutable release and pin its exact commit.
7. Add one owner-private adapter to `I2cTask` for the approved static board
   profile.
8. Add TunnelMonitor native tests, then exact-board shared-bus HIL.
9. Remove any superseded direct implementation for the same selected
   chip/profile. Retain a statically selected INA228 adapter for hardware 2.0.0
   if that board remains supported.
10. Only then version storage/cloud/UI schemas if the three-channel product
    contract was approved.

## Acceptance checklist

The library is suitable for TunnelMonitor when all of these are true:

- [ ] INA3221 board role, address, channels, shunts, timing, alerts, and data
      contract are approved.
- [ ] Integration work is based on v2.0.0 or its focused successor, not the old
      v1.2 checkout.
- [ ] Delay-only readiness uses datasheet maximum timing and fits the owner
      deadline.
- [ ] Binding/unbinding is zero-I2C; init/reinit/profile apply is cooperative.
- [ ] One staged engine owns hardware access and supports bus-silent cancel.
- [ ] Partial samples never replace or masquerade as the last completed batch.
- [ ] Every destructive Mask/Enable read retains alert evidence.
- [ ] Initialization and recovery establish one complete, verified profile.
- [ ] Measurement-profile uncertainty blocks or explicitly invalidates scaled
      output; alert-only uncertainty invalidates alert guarantees.
- [ ] Library health cannot suppress `I2cTask` owner-directed attempts.
- [ ] The normal result is fixed-unit, fixed-capacity, and coherent for every
      channel required by the approved result.
- [ ] Raw writes and status values cannot silently bypass production state.
- [ ] New native tests cover timing, alerts, cancellation, ambiguity, negative
      values, coherence, and transport shape.
- [ ] Exact TunnelMonitor native tests and the pinned ESP32-S3 Arduino firmware
      build pass; pure ESP-IDF also passes if the library release claims it.
- [ ] Exact-board shared-bus HIL passes with retained build identity/evidence.
- [ ] The selected library revision and its build toolchains are exact-pinned.
- [ ] No selected chip/profile retains both a direct and library implementation
      after qualification.

## Final recommendation

Adopt INA3221 v2.0.0 as the **refactor base**, not as an immediate production
dependency. Its protocol core is good enough to keep. The safest platform path
is a focused next release with maximum-time scheduling, one cancellable staged
engine, a complete verified profile, retained alert evidence, owner-managed
health, and an atomic fixed-unit result for the product-selected channels.

For TunnelMonitor, first approve a static INA3221 board profile and the data
shape. Then integrate the refactored library through the existing `I2cTask`
owner. This is simpler and more robust than adding register code, alert reads,
or recovery policy around the current library from the firmware side.
