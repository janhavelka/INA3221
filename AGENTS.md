# AGENTS.md - INA3221 Production Embedded Guidelines

## Role and Target
You are a professional embedded software engineer building a production-grade INA3221 triple-channel power monitor library.

- Target: ESP32-S2 / ESP32-S3, Arduino and ESP-IDF examples, PlatformIO / ESP-IDF component use.
- Goals: deterministic behavior, long-term stability, clean API contracts, portability, no surprises in the field.
- These rules are binding.

---

## Repository Model (Single Library)

```
include/INA3221/         - Public API headers only (Doxygen)
  CommandTable.h         - Register addresses and bit masks
  Status.h
  Config.h
  INA3221.h
  Version.h              - Auto-generated (do not edit)
src/                     - Implementation (.cpp)
examples/
  01_*/
  common/                - Example-only helpers (Log.h, BoardConfig.h, I2cTransport.h,
                           I2cScanner.h, CommandHandler.h)
platformio.ini
library.json
README.md
CHANGELOG.md
AGENTS.md
```

Rules:
- `examples/common/` is NOT part of the library. It simulates project glue and keeps examples self-contained.
- No board-specific pins/bus in library code; only in `Config`.
- Public headers only in `include/INA3221/`.
- Examples demonstrate usage and may use `examples/common/BoardConfig.h`.
- Public/core library headers and `src/` must not include Arduino or ESP-IDF framework headers.
- ESP-IDF examples must be native IDF applications using `app_main`,
  `driver/i2c_master.h`, `esp_timer`, FreeRTOS waits/yields, and fixed C
  buffers or native console APIs.
- ESP-IDF examples must not include Arduino CLI sources and must not use
  `Arduino.h`, `Wire.h`, `String`, `Serial`, `TwoWire`, `ArduinoCompat`, or
  `IdfArduinoCompat` facades.
- `examples/common/` remains Arduino-example glue; IDF-only adapters live under
  `examples/esp_idf/`.
- Keep the layout boring and predictable.

---

## Core Engineering Rules (Mandatory)

- Deterministic: no unbounded loops/waits; all timeouts via deadlines, never `delay()` in library code.
- Non-blocking lifecycle: `Status begin(const Config&)`, `void tick(uint32_t nowMs)`, `void end()`.
- Any I/O that can exceed ~1-2 ms must be split into state machine steps driven by `tick()`.
- No heap allocation in steady state (no `String`, `std::vector`, `new` in normal ops).
- No logging in library code; examples may log.
- No macros for constants; use `static constexpr`. Macros only for conditional compile or logging helpers.

---

## I2C Manager + Transport (Required)

- The library MUST NOT own I2C. It never touches `Wire` directly.
- `Config` MUST accept a transport adapter (function pointers or abstract interface).
- Transport errors MUST map to `Status` (no leaking `Wire`, `esp_err_t`, etc.).
- The library MUST NOT configure bus timeouts or pins.

---

## Status / Error Handling (Mandatory)

All fallible APIs return `Status`:

```cpp
struct Status {
  Err code;
  int32_t detail;
  const char* msg;  // static string only
};
```

- Silent failure is unacceptable.
- No exceptions.

---

## INA3221 Driver Requirements

- I2C address configurable: 0x40 (A0->GND), 0x41 (A0->VS), 0x42 (A0->SDA), 0x43 (A0->SCL).
- Check device presence in `begin()` by reading Manufacturer ID (0x5449) and Die ID (0x3220).
- Support 3 independent channels, each with shunt voltage and bus voltage measurement.
- Configurable channel enable/disable (CH1en, CH2en, CH3en).
- Configurable averaging: 1, 4, 16, 64, 128, 256, 512, 1024 samples.
- Configurable conversion times: 140µs, 204µs, 332µs, 588µs, 1.1ms, 2.116ms, 4.156ms, 8.244ms.
- Support operating modes:
  - **Power-down mode**: Minimum power consumption
  - **Single-shot mode**: Shunt only, bus only, or shunt+bus, then power-down
  - **Continuous mode**: Shunt only, bus only, or shunt+bus
- Alert support:
  - Critical alert limits per channel (compared against single conversion)
  - Warning alert limits per channel (compared against averaged value)
  - Shunt-voltage summation with selectable channels and sum limit
  - Power-valid upper/lower limits for bus voltage monitoring
- Conversion ready detection via CVRF flag in Mask/Enable register.
- Proper 13-bit signed result handling (two's complement, data in bits [15:3]).
- Shunt voltage LSB = 40µV, Bus voltage LSB = 8mV.
- Current and power calculation in host (no internal power register).
- Software reset via bit 15 of Configuration register.
- All registers volatile — no EEPROM/NVM.

---

## Driver Architecture: Managed Synchronous Driver

The driver follows a **managed synchronous** model with health tracking:

- All public I2C operations are **blocking** (no complex async - INA3221 has no EEPROM/NVM writes).
- `tick()` may be used for single-shot conversion wait or continuous mode polling.
- Health is tracked via **tracked transport wrappers** -- public API never calls `_updateHealth()` directly.
- Recovery is **manual** via `recover()` - the application controls retry strategy.

### DriverState (4 states only)

```cpp
enum class DriverState : uint8_t {
  UNINIT,    // begin() not called or end() called
  READY,     // Operational, consecutiveFailures == 0
  DEGRADED,  // 1 <= consecutiveFailures < offlineThreshold
  OFFLINE    // consecutiveFailures >= offlineThreshold
};
```

State transitions:
- `begin()` success -> READY
- Any I2C failure in READY -> DEGRADED
- Success in DEGRADED/OFFLINE -> READY
- Failures reach `offlineThreshold` -> OFFLINE
- `end()` -> UNINIT

### Transport Wrapper Architecture

All I2C goes through layered wrappers:

```
Public API (readShuntVoltage, readBusVoltage, etc.)
    ↓
Register helpers (readRegister16, writeRegister16)
    ↓
TRACKED wrappers (_i2cWriteReadTracked, _i2cWriteTracked)
    ↓  <- _updateHealth() called here ONLY
RAW wrappers (_i2cWriteReadRaw, _i2cWriteRaw)
    ↓
Transport callbacks (Config::i2cWrite, i2cWriteRead)
```

**Rules:**
- Public API methods NEVER call `_updateHealth()` directly
- `readRegister16()`/`writeRegister16()` use TRACKED wrappers -> health updated automatically
- `probe()` uses RAW wrappers -> no health tracking (diagnostic only)
- `recover()` tracks probe failures (driver is initialized, so failures count)

### Health Tracking Rules

- `_updateHealth()` called ONLY inside tracked transport wrappers.
- State transitions guarded by `_initialized` (no DEGRADED/OFFLINE before `begin()` succeeds).
- NOT called for config/param validation errors (INVALID_CONFIG, INVALID_PARAM).
- NOT called for precondition errors (NOT_INITIALIZED).
- `probe()` uses raw I2C and does NOT update health (diagnostic only).

### Health Tracking Fields

- `_lastOkMs` - timestamp of last successful I2C operation
- `_lastErrorMs` - timestamp of last failed I2C operation
- `_lastError` - most recent error Status
- `_consecutiveFailures` - failures since last success (resets on success)
- `_totalFailures` / `_totalSuccess` - lifetime counters (wrap at max)

---

## Versioning and Releases

Single source of truth: `library.json`. `Version.h` is auto-generated and must never be edited.

SemVer:
- MAJOR: breaking API/Config/enum changes.
- MINOR: new backward-compatible features or error codes (append only).
- PATCH: bug fixes, refactors, docs.

Release steps:
1. Update `library.json`.
2. Update `CHANGELOG.md` (Added/Changed/Fixed/Removed).
3. Update `README.md` if API or examples changed.
4. Commit and tag: `Release vX.Y.Z`.

---

## Naming Conventions

- Member variables: `_camelCase`
- Methods/Functions: `camelCase`
- Constants: `CAPS_CASE`
- Enum values: `CAPS_CASE`
- Locals/params: `camelCase`
- Config fields: `camelCase`
