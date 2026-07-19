# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

[Unreleased]: https://github.com/janhavelka/INA3221/compare/v3.0.0...HEAD
[3.0.0]: https://github.com/janhavelka/INA3221/compare/v2.0.0...v3.0.0
[2.0.0]: https://github.com/janhavelka/INA3221/compare/v1.2.0...v2.0.0
[1.2.0]: https://github.com/janhavelka/INA3221/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/janhavelka/INA3221/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/janhavelka/INA3221/releases/tag/v1.0.0
