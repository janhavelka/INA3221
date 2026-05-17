# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Added `SettingsSnapshot` and `getSettings(SettingsSnapshot&)` for cache-only config, conversion, Mask/Enable, and health inspection.
- Added `readConversionReady(bool&)` so conversion-ready polling can propagate I2C/status failures instead of collapsing them to `false`.
- Added bring-up CLI `cfg` / `settings` cached-settings output and decoded `mask` command.
- Added native coverage for triggered conversion gating, stalled-clock timeout handling, setter rollback, invalid register rejection, semantic recovery failures, and finite shunt validation.
- Added native coverage proving latched `OFFLINE` blocks normal I2C operations without touching the bus while `recover()` remains the explicit recovery path.

### Changed
- Doxyfile project metadata now matches `library.json`.
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

[Unreleased]: https://github.com/janhavelka/INA3221/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/janhavelka/INA3221/releases/tag/v1.0.0
