# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

All entries below resolve findings from the 2026-08-27 code audit of `3.1.0`
and its independent follow-up reviews.

### Added

- Added bus-silent `cancelConversion()`, which releases an outstanding legacy
  single-shot conversion without I2C, and exposed it as `cancel` in both
  diagnostic CLIs.
- Added `AlertFlags::timingControlFault` and `AlertSnapshot::timingControlFault`,
  the explicit TCF-low fault condition, alongside the existing raw TCF level.
- Added per-channel `CurrentDirection` to the legacy `Config`, mapped onto
  `DeviceProfile::shunts[i].direction` by `begin()` and reported back by
  `getConfig()`.
- Added an example-local fixed-name ESP-IDF component shim so the native example
  builds independently of the repository checkout directory name.

### Fixed

- Independent verification closed further Mask/Enable gaps: raw diagnostic
  reads now hand observed CVRF to legacy conversion state through the shared
  consuming-read helper. Summation-channel and alert-latch setters now consume
  and retain old flags before writing, as the datasheet requires; a failed
  pre-read prevents the write. These two setters use three bounded callbacks;
  Configuration and alert-limit setters still use two.
- Staged sample snapshots now retain observed conversion readiness during and
  after continuous or triggered reads, including a later channel-read failure.
  CVRF fault rechecks no longer move the recorded conversion start time.
- The timing guard recognizes prefixed C++ character literals (`L`, `u`, `U`,
  `u8`) so their closing quotes cannot hide a subsequent forbidden timing call.
- Updated the HIL suite for triggered blocking reads, legacy cancellation,
  verified-setter transfer counts, Wire-safe maximum poll budgets, timing-control
  diagnostics, and the shunt-sum selection precondition. The previous suite
  still expected successful sum reads with no summation channel selected.
- `readBlocking()` bounded its poll loop by iteration count rather than elapsed
  time, so a triggered read returned `CANCELLED` on any host fast enough to
  exhaust the budget before the conversion completed. It now runs against the
  absolute extended monotonic deadline with a stalled-clock guard that resets on
  time or transfer progress, and publishes capture uptime in the caller's
  absolute domain across the 32-bit clock wrap.
- Typed Configuration and alert setters marked the register unverified after a
  successful write, which left `measurementConfigState()` `DIRTY` and blocked
  every direct measurement read until a full `recover()`. All typed setters now
  share one write/readback verifier, commit staged profile state only after a
  matching read, and leave verified state `APPLIED`; failed or mismatched writes
  retain honest `DIRTY`/`UNKNOWN` certainty.
- An outstanding legacy conversion whose CVRF never arrived permanently rejected
  every owner job, `bind()` and `recover()`. Rebind and lifecycle/recovery jobs
  now abandon that stale bookkeeping bus-silently while taking over lifecycle
  and Configuration reconciliation; sample jobs still reject mixed ownership.
- The legacy float path ignored `ShuntCalibration::direction`, so `readCurrent()`
  and `SampleBatch` reported opposite signs on the same channel. Current, power
  and combined-channel reads now apply the profile direction without losing
  float precision.
- `readShuntSumRaw()` / `readShuntSumVoltage()` returned stale data as valid.
  They now require a shunt-measuring mode, at least one selected summation
  channel, and verified alert configuration, all rejected before I2C.
- Owner failure terminalization is centralized: interior deadline exhaustion is
  `TIMED_OUT`, a failure whose callback was never invoked cannot become an
  indeterminate write, and `PARTIAL` is reported only after a confirmed earlier
  write.
- Tracked transfers made before initialization succeeds now update the health
  counters, `lastError` and timestamps, so a failed first bring-up leaves usable
  diagnostics; only the `DriverState` transition stays gated on initialization.
- A confirmed software reset no longer fabricates a zero Mask/Enable reading. It
  clears the known power-on writable-bit cache; an ambiguous reset preserves the
  last real observation while certainty becomes `UNKNOWN`.
- Permanent-CVRF-low fault polling used a flat 1 ms recheck, saturating a shared
  bus with destructive Mask/Enable reads. It now rechecks every 50 ms and waits
  bus-silently when no further bounded read fits before the deadline.
- Mask/Enable typed setters composed their write from a destructive-read
  observation cache that a raw write or software reset could leave stale. They
  now compose the complete desired register from the retained profile and verify
  the readback.
- The Mask/Enable verification readback consumed CVRF without handing the
  observation to legacy conversion state, so a typed setter between a trigger
  and a read left readiness permanently false. The handoff `readAlertFlags()`
  performs is now shared by both paths.
- Every Arduino owner poll path, including the sampler used by `stress_owner`,
  now caps or pauses the transfer budget that its fixed Wire timeout can honor,
  and the adapter reports an unsupported tighter callback deadline as a
  callback-local configuration error rather than a fabricated bus timeout.
- `PollJobSnapshot::nextChannel` and `PollJobSnapshot::conversionStartMs` are
  published from the live cooperative-job cursor; both were backed by fields that
  were never written and always reported zero.
- The legacy observed Configuration view stays coherent across raw writes,
  host-only calibration changes, settings snapshots, readiness polling and
  explicit re-triggers. A definitely non-reaching Configuration write preserves
  active or completed conversion evidence; confirmed or ambiguous writes discard
  stale provenance. A successful typed write no longer overwrites the diagnostic
  explaining a pre-existing dirty or unknown register family.
- Float-to-register conversion now saturates in the float domain before
  `lrintf`, whose result is unspecified for out-of-range input. Legacy
  conversion accepts any invalid shunt representation on disabled channels,
  including zero, NaN and finite out-of-range values, while enabled channels
  still require representable finite positive calibration.
- Removed unreachable negative guards after masked power-valid decoding, and
  owner state members that were assigned but never read.
- The core timing checker now lexes comments, quoted literals, character
  literals and C++ raw strings, distinguishes digit separators from character
  literal openers, and runs an adversarial self-test instead of relying on
  substitution order. Each of those gaps could previously hide a forbidden
  timing call in core sources.
- Strengthened the CLI and ESP-IDF static contracts for the Wire error category,
  the transfer-budget helper, the component-shim depth, the renamed CI checkout
  path and the timing-control labels.
- Standardized both CLIs on `TCF` for the raw register field and
  `TimingControl` / `TimingControlFault` for decoded state, with the inverted
  `TC_FAULT` diagnostic made explicit.

### Changed

- `AlertSnapshot::timingControl` is now the latest observed raw TCF level
  instead of a sticky OR across reads, so a timing-control fault is reportable
  at all. The INA3221 latches TCF low in hardware until a power cycle or
  software reset, so the library no longer latches it a second time. The raw
  decode is unchanged: `true` still means TC high, that is, no fault.
- `profileGeneration()` now also advances after every typed setter whose write
  and readback verified, not only after a full profile job or
  `setShuntResistance()`. Code comparing a cached
  `SampleBatch::profileGeneration` must expect a bump from any successful setter.
- Cancelling or timing out a triggered sample no longer degrades
  `measurementConfigState()` to `DIRTY`. The trigger rewrites the already
  verified desired Configuration value, so a confirmed trigger write cannot
  change managed state. Power-down still reports `DIRTY` after a confirmed write
  and now also raises `hardwareConfigDirty()`.
- `AlertSnapshot::evidenceUncertain` is no longer set when a Mask/Enable read
  definitely did not reach the device: an address-phase NACK,
  `DEVICE_NOT_FOUND`, a callback-local `INVALID_CONFIG`/`INVALID_PARAM`
  rejection, or a driver-side abort before any callback. Adapters must report an
  accurate failure phase for this classification to hold.
- Corrected public Doxygen contracts that did not match the implementation:
  `readConversionReady()` (never returns `CONVERSION_NOT_READY`), `end()`
  (bus-silent, performs no power-down), `softReset()` and a reset-bit
  `writeConfig()` (a confirmed reset leaves certainty `DIRTY`, not `UNKNOWN`),
  `writeRegister16()` (every accepted or ambiguous raw write invalidates its
  certainty family), `profileGeneration()`, `cancelJob()`, `bind()`,
  `SampleBatch::validChannels`, `PollJobSnapshot::conversionStartMs`, the
  Mask/Enable setters, and the legacy observed-configuration getters.
- Documented verified-setter callback counts, initial health telemetry,
  power-down retention, timing-control semantics and the device-side TCF latch,
  non-atomic live profile application, fixed-timeout adapter admission, legacy
  cancellation, per-family certainty promotion, and the provenance of reserved
  and transport-supplied error codes.
- Clarified that callback-local validation may be bus-silent, that owner
  transfer counts measure callback invocations rather than guaranteed physical
  bus attempts, and which failure phases are treated as definitely not having
  reached the device.
- Replaced the stale v2 "managed synchronous driver" architecture description in
  `AGENTS.md` with the current cooperative owner engine, and corrected the
  repository layout, the lifecycle rule, the `recover()` health note, the
  pre-initialization counter rule, and the saturating-counter description.

## [3.1.0] - 2026-08-05

### Added

- Added complete Arduino and native ESP-IDF diagnostic CLI coverage for all
  cooperative job kinds and progress fields, runtime address selection and
  initialization, valid-address discovery, runtime I2C frequency changes,
  retained fixed-unit samples and alert evidence, current-direction policy,
  managed-register verification, transfer counters/assertions, deterministic
  HIL framing, owner-job stress, and frequency-switching stress.
- Added cache-only full diagnostics with complete last transport error
  code/detail/message/time, desired-profile certainty, owner result/progress,
  retained verification evidence, and physical transfer totals.
- Added full register mismatch evidence to `JobResult` (register, expected,
  actual, comparison mask) for cooperative profile and power-down verification
  failures, with native tests for both paths.
- Added a maintained HIL guide and reviewed evidence ledger with bounded
  commands, explicit coverage, fixture assumptions, and non-claims.
- Added a PlatformIO post-builder upload-configuration hook that disables
  esptool `5.3.0`'s Unicode progress bar on legacy Windows code pages.

### Changed

- Updated Arduino example and HIL builds from exact pioarduino `54.03.20`
  (Arduino-ESP32 `3.2.0`, ESP-IDF `5.4.1`) to `55.03.311`
  (Arduino-ESP32 `3.3.11`, ESP-IDF `5.5.5`), made the ESP32-S3 4 MB QIO
  flash/2 MB QSPI PSRAM layout explicit, and removed the legacy ESP32
  ECO1-only PSRAM cache workaround flag.
- Restored the ESP32-S3 example to USB Serial/JTAG HWCDC and qualified its
  single-port upload/CLI path under Arduino-ESP32 `3.3.11`, preserving one
  stable COM-port identity for long HIL sessions.
- Updated GitHub Actions to the current Node 24 action majors and preserved the
  `INA3221` component directory name in ESP-IDF container builds.
- Consolidated maintained documentation under `docs/` and removed completed
  audits, dated validation reports, task prompts, generated extraction archives,
  and implementation-era research material.
- Expanded the hardware-validation runner across every operating mode,
  averaging and conversion-time value, all three channel configuration paths,
  direct measurements on all channels, cooperative sample/cancel jobs,
  alert and raw-register write/readback/restore paths, CLI aliases and invalid
  inputs, exact configuration-register encodings, exact stress totals, and
  strict final/soak health invariants.
- Switched automated CLI completion detection from prompt timing to explicit
  `HIL_BEGIN`/`HIL_END` frames and expanded the bounded suite from 286 to 379
  steps before the optional three benchmarks.
- Exposed the public alternate power-down encoding through the Arduino and
  native ESP-IDF example CLIs as `mode pda` and added exact HIL readback.
- Reorganized and expanded the README with hardware/address guidance, profile
  validation/defaults, the complete cooperative job lifecycle, fixed-unit
  sample semantics, alert/configuration certainty, error recovery, installation,
  examples, generated-documentation instructions, and a documentation map.
- Added focused Arduino/PlatformIO and native ESP-IDF example runbooks covering
  board configuration, build/flash commands, demonstrated ownership flow, CLI
  entry points, and the boundary between build and HIL evidence.
- Completed public-header Doxygen coverage for production and compatibility
  APIs, data/result types, utility conversions, transport/profile fields,
  register constants, and generated version macros.
- Made Doxygen omit internal engineering/example implementation surfaces and
  fail on undocumented public symbols or missing parameter documentation.
- Updated the native ESP-IDF guide to describe v3 as the current production API.
- Replaced stale contribution and security guidance with current validation,
  release, disclosure, and supported-version policies.
- Removed uncalled Arduino example wrappers, duplicated CLI/log formatting,
  obsolete native-test Arduino/Wire stubs, and TunnelMonitor-only version
  generator logic that could never run in this repository.

### Removed

- Removed synthesized application-note summaries, supplemental research PDFs,
  generated Markdown/plain-text extraction archives, compact implementation
  notes, and the completed chip implementation manual. The authoritative
  INA3221 datasheet and maintained library/integration documentation remain.

### Fixed

- Initialized the Arduino `Wire` bus atomically at the requested frequency,
  avoiding pioarduino `55.03.311`'s false `setClock()` failure before any
  device handles exist, and verified the applied clock before publishing the
  example transport context.
- Increased the bounded example owner-job deadline to five seconds so manual
  progress inspection and exact-budget stepping do not expire healthy jobs.
- Fixed the Doxygen main-page collision and made the ESP-IDF CI container keep
  the local component's `INA3221` directory name.
- Avoided flushing the two-byte CLI prompt on ESP32-S3 HWCDC, where a
  transient SOF connection-state flap can make `HWCDC::flush()` discard the
  queued prompt. The prompt now yields to the USB Serial/JTAG TX ISR while
  TinyUSB retains the existing flush behavior.
- Made the default `readBlocking()` deadline derive from the active profile and
  transport callback bound, so the default 50 ms per-transfer ceiling cannot
  make a healthy three-channel triggered read fail admission against the former
  fixed 200 ms deadline.
- Removed stale documentation that referred to the released v3 API as a future
  major version or claimed `0.1.x` was the supported release.
- Corrected contributor guidance to reference the tracked `.clang-format`, and
  corrected the HIL runner's `pyserial` installation guidance and strict final
  health classification.

## [3.0.0] - 2026-07-19

### Added
- Added the production `TransportConfig` and complete fixed-size
  `DeviceProfile`, including integer shunt calibration/direction and the full
  managed alert profile.
- Added zero-I2C `bind()` and bus-silent `unbind()` lifecycle operations.
- Added one cooperative job engine for staged initialize, apply-profile,
  reconcile, triggered/continuous sampling, and verified power-down operations.
- Added `PollContext` with absolute monotonic deadlines, per-transfer timeouts,
  and a strict callback budget; one poll never exceeds `maxTransfers`.
- Added cache-only `JobProgress` and take-once `JobResult` with request identity,
  terminal state, transfer count, profile generation, and explicit hardware
  effect.
- Added fixed-unit `SampleBatch` results with three fixed channel slots,
  per-quantity/channel validity, coherence, capture uptime, profile generation,
  request ID, and alert snapshot provenance.
- Added retained destructive alert events with peek/take access, and separate
  measurement/alert configuration certainty (`UNKNOWN`, `APPLIED`, `DIRTY`).
- Added pure fixed-unit conversion, calibration, timing, register-policy, and
  successful-path maximum-transfer helpers.
- Added deterministic owner-engine and fault-injection coverage plus strict
  compiler, metadata consistency, Doxygen warning, and compiled ESP-IDF CI
  gates.

### Changed
- Made the shared-bus production path explicitly single-owner, serialized,
  non-reentrant, non-ISR, non-copyable, and non-movable. The library owns no bus,
  task, lock, retry/recovery policy, or deadline renewal.
- Defined each transport callback as exactly one bounded physical attempt with
  exact-length completion, no hidden retry/recovery/reconfiguration, and no
  driver re-entry.
- Made READY/DEGRADED/OFFLINE health passive telemetry; OFFLINE no longer acts
  as an I2C admission gate.
- Changed triggered conversion scheduling to use maximum conversion timing plus
  a fixed 100 us wake margin from a strictly post-callback time origin before
  the CVRF/alert read, with bus-silent deadline-fit admission.
- Bounded the staged compatibility APIs with derived deadlines, extended their
  32-bit poll clock across wrap, and made `readBlocking()` use wrap-safe elapsed
  time.
- Updated Arduino and native ESP-IDF examples to lead with budget-one staged
  initialization, triggered fixed-unit sampling, progress, bus-silent cancel,
  and take-once result handling while retaining the diagnostic CLI.
- Version-pinned PlatformIO and ESP-IDF build inputs and added real
  ESP32-S3/ESP32-S2 IDF compiler jobs in CI.
- Reclassified `Config`, `begin()`, direct measurement/configuration calls,
  `readBlocking()`, `probe()`, and `recover()` as bounded standalone/diagnostic
  compatibility APIs rather than the recommended shared-owner steady path.

### Fixed
- Preserved latched alert events across destructive Mask/Enable reads until the
  application explicitly takes them.
- Exposed uncertainty when a failed or short Mask/Enable transfer may already
  have destructively cleared alert evidence.
- Prevented terminal-result overwrite and request reuse before exactly-once
  result consumption.
- Exposed partial and indeterminate hardware effects after confirmed or
  ambiguous writes instead of reporting implicit rollback or fake success.
- Removed health-state suppression of owner-admitted transport operations.
- Rejected compatibility setter changes that would leave enabled-channel,
  calibration, or alert-summation profile invariants invalid before any I2C.

## [2.0.0] - 2026-06-29

### Added
- Added `tickStatus(nowMs)` for callers that need the bounded `tick()` work
  plus an observable `Status` result.
- Added `powerDown()` for verified shutdown writes while keeping `end()` as a
  best-effort teardown API.
- Added staged polling APIs for deadline-owned I2C loops:
  `startSingleShot()`, `pollSingleShot()`, `startContinuousRead()`,
  `pollContinuousRead()`, `pollJob()`, `getPollJobSnapshot()`,
  `readChannelRawStep()`, `readAndClearAlertFlags()`,
  `startApplyMaskEnable()`, and `pollApplyMaskEnable()`.
- Added `driverState()` as a source-compatible alias for `state()`.
- Added `hardwareConfigDirty()` / `hardwareConfigDirtyStatus()` and matching
  `SettingsSnapshot` fields for Configuration and Mask/Enable cache
  uncertainty diagnostics.
- Added `tools/hil_cli_runner.py` for bounded serial CLI HIL runs with parser
  self-test, dry-run, transcript capture, Markdown reporting, benchmark steps,
  and optional soak duration limits.
- Added HIL runner DTR/RTS line-state options for native USB CDC serial
  endpoints that require DTR before they emit CLI output.
- Native coverage for probe transport-error preservation, disabled and
  mode-inactive measurement reads, `tick()` Mask/Enable read-clear behavior,
  and bus-voltage conversion boundaries.
- Native budget coverage for single-instruction, two-instruction, and draining
  staged sample polls, disabled-channel skipping, destructive readiness reads,
  continuous reads, and staged Mask/Enable writes.
- Native coverage for `tickStatus()` I2C error propagation, `powerDown()`
  success/failure behavior, `readBlocking()` validation, raw Mask/Enable
  read-clear side effects, and recover split-write failure diagnostics.

### Changed
- Documented `readBlocking()` and `conversionReady()` as convenience APIs for
  non-steady paths, with staged polling APIs preferred for deadline-owned I2C
  owners.
- `readBlocking()` now rejects calls with no output channels and rejects
  timeout values too large for wrap-safe local deadline math.
- `setShuntResistance()` is now runtime-only; pre-begin shunt values must be
  supplied through `Config::shuntResistance`.
- Raw `readRegister16(REG_MASK_ENABLE)` side effects are documented alongside
  typed Mask/Enable readers.
- Documented the serial HIL runner and its fixture limitations.
- The HIL runner now uses more conservative default command pacing, a less
  diagnostic-heavy soak cycle, and fail-fast handling when prompt resync fails.
- ESP32-S3 Arduino example builds now use TinyUSB CDC (`ARDUINO_USB_MODE=0`)
  instead of USB Serial/JTAG HWCDC for stable long-running HIL output.
- Consolidated generated HIL reports into a maintained summary and removed full
  transcripts/PID artifacts from release docs.

### Fixed
- `probe()` now preserves raw transport errors instead of collapsing I2C
  timeout, bus, or NACK failures to `DEVICE_NOT_FOUND`.
- Per-channel measurement reads now reject disabled channels and quantities not
  produced by the active mode before touching I2C.
- Failed Configuration or Mask/Enable cache writes, including split recover
  failures, now expose cache/hardware uncertainty instead of silently relying on
  rolled-back cached state.
- Arduino CLI diagnostics now flush and yield during long multi-line responses
  to avoid USB CDC output backpressure during HIL soak runs.
- Avoided ESP32-S3 HWCDC serial stalls observed during long HIL soak runs by
  validating the example on TinyUSB CDC with explicit DTR.

## [1.2.0] - 2026-05-20

### Added
- ESP-IDF component metadata, root `CMakeLists.txt`, and a native
  `examples/esp_idf/basic` application using the ESP-IDF new I2C master driver
  with the same user-visible CLI as the Arduino example.
- Native ESP-IDF fixed-buffer command shell with IDF-owned timing, waits, bus
  scan, identity-read, diagnostics, stress, self-test, and I2C adapter glue.
- `tools/check_idf_example_contract.py` to guard ESP-IDF example structure,
  native-driver dependencies, and CLI parity.
- IDF port implementation notes documenting the framework-neutral core boundary
  and validation status.

### Changed
- Removed Arduino `millis()` and `yield()` fallbacks from the driver core.
  Applications should provide `Config::nowMs` and `Config::cooperativeYield`
  when blocking helpers need wall-clock time or cooperative scheduling.
- Declared `espidf` framework support in PlatformIO metadata while keeping the
  Arduino example functionality equivalent through example-local hooks.
- The ESP-IDF example now exposes the same commands, help, three-channel
  measurements, conversion controls, alert limits, raw-register diagnostics,
  scanner identity checks, health, stress, and self-test workflows as the
  Arduino CLI.
- `examples/common/` is Arduino-only example glue; ESP-IDF uses
  `examples/esp_idf/basic/main/Ina3221IdfI2cTransport.*` directly.
- `tools/check_idf_example_contract.py` now rejects Arduino compatibility
  facades, Arduino CLI source inclusion, and Arduino framework tokens in IDF
  example code.
- Release metadata, README installation instructions, and Doxygen project
  metadata now target `v1.2.0`.

### Removed
- Removed the ESP-IDF path's `IdfArduinoCompat.h` compatibility facade and
  shared Arduino CLI-source inclusion.

### Fixed
- Corrected validation notes: Arduino example behavior has owner hardware-test
  coverage, while ESP-IDF build and hardware validation remain pending.

## [1.1.0] - 2026-05-17

### Added
- Added `SettingsSnapshot` and `getSettings(SettingsSnapshot&)` for cache-only config, conversion, Mask/Enable, and health inspection.
- Added `readConversionReady(bool&)` so conversion-ready polling can propagate I2C/status failures instead of collapsing them to `false`.
- Added bring-up CLI `cfg` / `settings` cached-settings output and decoded `mask` command.
- Added INA3221 identity recognition to the example I2C scanner by checking `0x40`-`0x43` for Manufacturer ID `0x5449` and Die ID `0x3220`.
- Added no-argument show forms for bring-up CLI `chen`, `rshunt`, `crit`, `warn`, `sumch`, and `latch`.
- Added native coverage for triggered conversion gating, stalled-clock timeout handling, setter rollback, invalid register rejection, semantic recovery failures, and finite shunt validation.
- Added native coverage proving latched `OFFLINE` blocks normal I2C operations without touching the bus while `recover()` remains the explicit recovery path.

### Changed
- Updated release metadata to `1.1.0` in `library.json`, generated `Version.h`, and Doxygen project metadata.
- Doxyfile project metadata now matches `library.json`.
- Bring-up CLI numeric and boolean arguments now use strict parsing instead of `String::toInt()` fallbacks for `verbose`, `read`, `avg`, conversion-time setters, channel enable, summation channel, latch, and stress commands.
- `stress_mix` now labels health deltas as tracked I2C transactions to distinguish them from high-level operation counts.
- Explicit recovery bypass internals now use the shared `ScopedOfflineI2cAllowance` / `_reassertOfflineLatch()` procedure so failed recovery attempts that begin from `OFFLINE` keep the latch asserted.
- Reference documentation now uses human-readable vendor PDF names and separates compact power-monitor notes from full PDF/application-note extractions under `docs/extracted-md/` and `docs/pdf-extracted-md/`.
- Completed Doxygen parameter documentation for shunt-resistance configuration.
- Triggered-mode configuration writes are now tracked as conversion starts, including `begin()`, `setMode()`, `writeConfig()`, and `startConversion()`.
- Measurement reads in triggered mode now return `CONVERSION_NOT_READY` until the configured conversion cycle has elapsed and CVRF is observed.
- Configuration setters now rollback cached driver state when the underlying I2C write fails.
- Alert-limit setters now clear reserved bits before register writes, Mask/Enable writable cache survives config writes, and reset-bit `writeConfig()` synchronizes cached defaults.
- Failed `begin()` clears stale runtime/health state before validation.
- `readBlocking()` now has a bounded polling escape even if the injected clock callback stops advancing.
- Raw register helpers now reject addresses outside `0x00`-`0x11`, `0xFE`, and `0xFF` before touching I2C.
- Shunt resistance validation now rejects non-finite values.
- Health behavior is now standardized on latched `OFFLINE`: normal public I2C operations return `BUSY` with `Driver is offline; call recover()` and do not touch I2C until `recover()` succeeds.

### Fixed
- Fixed implementation-defined signed right shifts when converting INA3221 13-bit and summation register fields.
- Fixed `recover()` so manufacturer/die ID mismatches update health counters and state.
- Avoided read-modify-write of Mask/Enable settings for latch/summation configuration, preventing configuration helpers from clearing alert/CVRF flags just to update writable mask bits.
- Fixed bring-up CLI help/dispatch mismatches that reported advertised bare commands as unknown.
- Fixed clean-checkout packaging by committing generated `Version.h` instead of ignoring the public header required by `INA3221.h`.

## [1.0.0] - 2026-04-05

### Added
- Initial driver implementation for INA3221 triple-channel power monitor.
- Full register map in `CommandTable.h` (config, shunt/bus voltage, alert limits, mask/enable, ID).
- `Status`/`Err` error handling with I2C sub-codes (`I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`, `I2C_BUS`).
- `Config` struct with I2C transport callbacks, timing hooks, per-channel shunt resistance, and all conversion settings.
- Lifecycle: `begin()`, `tick()`, `end()`.
- Health tracking: `DriverState` (UNINIT/READY/DEGRADED/OFFLINE), consecutive/total failure counters, timestamps.
- Transport wrapper architecture: raw/tracked layers, health updated only in tracked wrappers, `probe()` uses raw (no health side-effects).
- Measurement API: `readShuntRaw()`, `readBusRaw()`, `readShuntVoltage()`, `readBusVoltage()`, `readCurrent()`, `readPower()`, `readChannel()`, `readShuntSumRaw()`, `readShuntSumVoltage()`.
- Single-shot conversion API: `startConversion()`, `conversionReady()`, `readBlocking()`.
- Configuration API: `setMode()`, `setAveraging()`, `setVBusConvTime()`, `setVShuntConvTime()`, `setChannelEnable()`, `setShuntResistance()`, `readConfig()`, `writeConfig()`, `softReset()`.
- Alert API: critical/warning limits per channel, shunt-sum limit, power-valid upper/lower limits, `readAlertFlags()`, `setSummationChannels()`, `setAlertLatchEnable()`.
- Device identification: `readManufacturerId()` (0x5449), `readDieId()` (0x3220).
- Utility: `shuntRawToMv()`, `busRawToVolts()`, `mvToShuntRaw()`, `voltsToBusRaw()`, `getConversionTimeUs()`, `getCycleTimeUs()`.
- Auto-generated `Version.h` from `library.json` via `scripts/generate_version.py`.
- Native Unity tests (`test/test_basic.cpp`) with Arduino/Wire stubs.
- Interactive CLI bringup example (`examples/01_basic_bringup_cli/`).
- Example helpers: `BoardConfig.h`, `I2cTransport.h`, `I2cScanner.h`, `Log.h`, `BusDiag.h`, `CliShell.h`, `HealthView.h`.
- Public lifecycle/config introspection helpers: `isInitialized()` and `getConfig()`.
- Public tracked raw-register helpers: `readRegister16()` and `writeRegister16()`.
- Cross-library conversion-time naming aliases: `setVbusConvTime()` / `getVbusConvTime()` and `setVshuntConvTime()` / `getVshuntConvTime()`.
- `Err::MEASUREMENT_NOT_READY` alias for cross-library uniformity.
- Bringup CLI register diagnostics plus richer `stress` / `stress_mix` reporting.

### Changed
- `end()` now best-effort powers the monitor down before clearing runtime state.
- `recover()` now re-validates manufacturer / die IDs, clears conversion state, and reapplies cached configuration.

[Unreleased]: https://github.com/janhavelka/INA3221/compare/v3.1.0...HEAD
[3.1.0]: https://github.com/janhavelka/INA3221/compare/v3.0.0...v3.1.0
[3.0.0]: https://github.com/janhavelka/INA3221/compare/v2.0.0...v3.0.0
[2.0.0]: https://github.com/janhavelka/INA3221/compare/v1.2.0...v2.0.0
[1.2.0]: https://github.com/janhavelka/INA3221/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/janhavelka/INA3221/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/janhavelka/INA3221/releases/tag/v1.0.0
