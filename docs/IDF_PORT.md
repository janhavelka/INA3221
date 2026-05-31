# INA3221 ESP-IDF v6.0.1 Port Audit

Last audited: 2026-05-19

This started as a readiness audit and now records the merged ESP-IDF
implementation on `main`. The driver core is framework-neutral, and the native ESP-IDF
example exposes the same user-visible CLI as the Arduino example. See
`docs/IDF_PORT_IMPLEMENTATION.md` for the implemented file-level summary and
validation notes.

Official ESP-IDF references:
- I2C master driver: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2c.html
- ESP-IDF v6.0 peripheral migration guide: https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/migration-guides/release-6.x/6.0/peripherals.html

## Current Framework/Library State

- `library.json` version is `1.2.0`; the package declares `arduino` and
  `espidf` framework support on `espressif32`.
- `platformio.ini` builds the Arduino CLI example for ESP32-S3 and ESP32-S2 and
  includes a native Unity test environment.
- Public API is under `include/INA3221/` and is already callback-based at the
  I2C boundary.
- `include/INA3221/Config.h` exposes `I2cWriteFn`, `I2cWriteReadFn`,
  optional `NowMsFn`, and optional `YieldFn`.
- `include/INA3221/INA3221.h` exposes `Status begin(const Config&)`,
  `void tick(uint32_t nowMs)`, `void end()`, three-channel measurement APIs,
  single-shot conversion helpers, alert limit APIs, raw register access, and
  four-state health tracking.
- `src/INA3221.cpp` routes I2C through `_i2cWriteReadRaw`, `_i2cWriteRaw`,
  `_i2cWriteReadTracked`, and `_i2cWriteTracked`; health is updated from
  tracked wrappers.
- The library core no longer includes `<Arduino.h>` and no longer calls
  `millis()` or `yield()` from `_nowMs()` / `_cooperativeYield()`; applications
  should provide timing/yield hooks when needed.
- Arduino-only glue lives in `examples/common/I2cTransport.h`,
  `I2cScanner.h`, `BoardConfig.h`, and the CLI example. ESP-IDF glue lives
  under `examples/esp_idf/basic/main/`.

Readiness verdict: the driver core is framework-neutral and the ESP-IDF example
uses the new I2C master driver plus the full bring-up CLI. Final readiness still
requires an ESP-IDF 6.0.1 build and hardware validation.

## Portability Blockers

- ESP-IDF compilation has not been verified in this shell because `idf.py` was
  unavailable.
- Hardware validation remains outstanding.
- Arduino example behavior has owner hardware-test coverage and remains the
  reference behavior for parity checks in this pass.
- ESP-IDF command parity is implemented in a separate native command shell.
  Arduino builds use `Serial`, `String`, `Wire`, `millis()`, `delay()`, and
  `yield()` only in Arduino examples.
- `readConversionReady()` reads the Mask/Enable register, which clears CVRF and
  latched alert flags. IDF examples and tests must call this deliberately.
- IDF v6.0.1 warning profiles can expose implicit conversion, signed/unsigned,
  and unused-variable warnings.

## Exact Files/APIs Changed

- `src/INA3221.cpp`
  - Removed the unconditional `#include <Arduino.h>`.
  - Keep `_i2cWriteReadRaw()`, `_i2cWriteRaw()`,
    `_i2cWriteReadTracked()`, and `_i2cWriteTracked()` as the only transport
    path.
  - `_nowMs()` and `_cooperativeYield()` no longer call framework fallbacks;
    applications provide `Config::nowMs` and `Config::cooperativeYield` when
    timestamps, blocking helpers, or cooperative scheduling matter.
  - Do not add direct `i2c_master_*` calls to measurement or register helpers.
- `include/INA3221/Config.h`
  - Preserve the current callback signatures; they are already ESP-IDF
    compatible.
  - Document that IDF examples should set `nowMs` and `cooperativeYield`.
  - Do not include IDF driver headers in this public core header.
- `include/INA3221/INA3221.h`
  - Preserve namespace, class name, enums, `Status`, channel APIs, alert APIs,
    and health APIs.
- Added root `CMakeLists.txt`.
- Added IDF-only adapter/example files under `examples/esp_idf/basic/`.
- Removed the ESP-IDF compatibility facade and Arduino CLI-source inclusion.
  The IDF entry point now uses native IDF APIs directly.

## Proposed Architecture Preserving Arduino Compatibility

- Keep the INA3221 core callback-based and framework-neutral.
- Keep `examples/common/I2cTransport.h` as the Arduino `Wire` adapter.
- Keep the IDF adapter outside the driver core. It owns IDF bus/device handles,
  supports address-window identity reads for the scanner, and supplies callbacks
  to `INA3221::Config`.
- Keep I2C bus configuration in the application/example: SDA, SCL, clock speed,
  pull-ups, and lifetime.
- Preserve health behavior:
  - `probe()` uses raw I2C and does not update health;
  - public register/measurement helpers use tracked wrappers;
  - invalid config/params and `NOT_INITIALIZED` are not transport failures;
  - `recover()` uses tracked operations.
- Keep CLI parity through `tools/check_idf_example_contract.py`, not by sharing
  Arduino implementation source.

## IDF Transport Adapter Contract

The adapter should use the ESP-IDF v6.0.1 new I2C master driver only:

```cpp
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

struct Ina3221IdfI2c {
  i2c_master_bus_handle_t bus = nullptr;
  i2c_master_dev_handle_t dev = nullptr;
  uint8_t address = 0x40;
};
```

Callback behavior:
- `i2cWrite(addr, data, len, timeoutMs, user)` calls
  `i2c_master_transmit(dev, data, len, timeoutMs)`.
- `i2cWriteRead(addr, txData, txLen, rxData, rxLen, timeoutMs, user)` calls
  `i2c_master_transmit_receive(dev, txData, txLen, rxData, rxLen, timeoutMs)`.
- The callbacks must be synchronous from the driver point of view. Do not
  register `i2c_master_register_event_callbacks()` on this handle unless the
  adapter waits for completion before returning; tracked wrappers update health
  immediately after the callback returns.
- Reject addresses other than the configured device address. INA3221 valid
  addresses are `0x40` through `0x43`.
- Map `ESP_OK` to `INA3221::Status::Ok()`.
- Map `ESP_ERR_TIMEOUT` to `INA3221::Err::I2C_TIMEOUT`.
- Map `ESP_ERR_INVALID_RESPONSE` to an I2C NACK-related status. The simple
  ESP-IDF master APIs do not distinguish address and data phase, so prefer
  `INA3221::Err::I2C_ERROR` with `Status.detail = ESP_ERR_INVALID_RESPONSE`
  unless a custom adapter can prove the phase.
- Map other adapter or bus failures to `INA3221::Err::I2C_BUS` or
  `INA3221::Err::I2C_ERROR`; preserve raw `esp_err_t` in `Status::detail`.
- Clamp or reject `timeoutMs` before passing it to ESP-IDF's signed
  `xfer_timeout_ms`; never allow overflow to become `-1` because `-1` waits
  forever.
- `nowMs(user)` should return `esp_timer_get_time() / 1000`.
- `cooperativeYield(user)` should call `taskYIELD()` or `vTaskDelay(1)` based
  on the example's scheduling policy.

## Component/CMake Layout

Recommended component layout:

```text
INA3221/
  CMakeLists.txt
  include/INA3221/*.h
  src/INA3221.cpp
  examples/esp_idf/basic/
    CMakeLists.txt
    main/CMakeLists.txt
    main/main.cpp
    main/Ina3221IdfI2cTransport.cpp
```

Core-only component:

```cmake
idf_component_register(
  SRCS "src/INA3221.cpp"
  INCLUDE_DIRS "include"
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
```

The adapter currently lives only in the example. Its component declares
`REQUIRES INA3221 esp_driver_i2c esp_driver_gpio esp_timer freertos`.

## Example Plan

- Keep the existing Arduino CLI example building with PlatformIO for ESP32-S2
  and ESP32-S3.
- `examples/esp_idf/basic/main/main.cpp` defines `app_main`, uses fixed C
  buffers for command input, calls `driver/i2c_master.h` through the IDF
  adapter, uses `esp_timer_get_time()` for timing, and uses FreeRTOS waits.
- The ESP-IDF CLI exposes the same help grouping, scanner
  identity checks, three-channel measurements, conversion controls, alert
  limits, raw registers, health, probe/recover, stress, and self-test flows as
  the Arduino CLI.

## Test/Validation Plan

- Static checks:
  - `python tools/check_cli_contract.py`
  - `python tools/check_idf_example_contract.py`
  - `python tools/check_core_timing_guard.py`
  - `rg "<Arduino.h>|<Wire.h>|millis\\(|delay\\(|yield\\(" include src`
    should find no unguarded Arduino dependencies in the ESP-IDF build path.
  - `python tools/check_idf_example_contract.py` rejects Arduino compatibility
    facades and Arduino CLI source inclusion in IDF examples.
  - `rg "driver/i2c.h|i2c_cmd_link|i2c_driver_install" include src examples CMakeLists.txt`
    should not find legacy I2C driver usage in IDF code.
- Arduino regression:
  - `pio test -e native`
  - `pio run -e esp32s3dev`
  - `pio run -e esp32s2dev`
- IDF build:
  - `idf.py set-target esp32s3 build` from `examples/esp_idf/basic`
  - `idf.py set-target esp32s2 build` from `examples/esp_idf/basic`
- Hardware validation:
  - `begin()` verifies manufacturer ID `0x5449` and die ID `0x3220`.
  - Read shunt and bus raw values for all three channels and validate signed
    13-bit decoding.
  - Validate current and power scaling from configured shunt resistances.
  - Validate conversion-ready behavior and document Mask/Enable clear effects.
  - Validate alert limit writes/readbacks and summation channel configuration.
  - Inject NACK, timeout, and bus errors and verify health transitions and
    `recover()`.

## ESP-IDF v6.0.1 Migration Hazards

- Do not use legacy `<driver/i2c.h>` or command-link APIs. New code must use
  `<driver/i2c_master.h>` and declare `esp_driver_i2c`.
- `ESP_ERR_INVALID_RESPONSE` is the new-driver NACK indication; map it
  consistently and keep the numeric detail.
- ESP-IDF components must declare split driver dependencies explicitly.
- The I2C callback timeout is milliseconds for the new master API. Clamp it to
  a finite signed millisecond value and do not pass FreeRTOS ticks by mistake.
- IDF v6 warning-as-error profiles can fail on integer conversions in register
  packing/unpacking. Fix warnings before CI gating.
- Keep bus ownership outside the INA3221 driver.

## Ordered Implementation Checklist

1. Done: add the root `CMakeLists.txt` for the core component.
2. Done: remove the Arduino include and timing/yield fallbacks in
   `src/INA3221.cpp`.
3. Done: add the IDF I2C adapter using `<driver/i2c_master.h>`.
4. Done: add `examples/esp_idf/basic` with a native full CLI.
5. Done: add static Arduino/IDF CLI contract checks.
6. Pending local ESP-IDF toolchain: build `examples/esp_idf/basic` for ESP32-S3.
7. Pending local ESP-IDF toolchain: build `examples/esp_idf/basic` for ESP32-S2.
8. Done: run PlatformIO native and Arduino example builds as regression checks.
9. Validate identity, three-channel measurements, conversion-ready behavior,
   and alert APIs on hardware.
10. Inject I2C failures and verify status/health/recovery behavior.
11. Add final `espidf` metadata/build matrix coverage and keep generated
    `Version.h` synchronized with `library.json`.
