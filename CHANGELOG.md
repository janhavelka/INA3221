# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
