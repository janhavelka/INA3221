# INA3221 ESP-IDF Port Implementation

Implemented on branch `idf-port`.

## Core Boundary

- `include/` and `src/` are framework-neutral and do not include Arduino,
  `Wire`, `Serial`, ESP-IDF I2C, FreeRTOS, or GPIO headers.
- The driver receives all I2C access through `Config::i2cWrite` and
  `Config::i2cWriteRead`.
- `Config::nowMs` and `Config::cooperativeYield` remain optional. If `nowMs` is
  not supplied, health timestamps use `0`; if `cooperativeYield` is not supplied,
  the driver performs no scheduler call.

## ESP-IDF Additions

- Root `CMakeLists.txt` registers the library as an ESP-IDF component.
- `idf_component.yml` declares component-manager metadata for ESP-IDF 6.x.
- `examples/esp_idf/basic` demonstrates application-owned bus/device setup with
  the new `driver/i2c_master.h` API, `esp_timer_get_time()` timing, and a
  FreeRTOS yield hook.

## Validation

- Static check target: `rg "<Arduino.h>|<Wire.h>|millis\\(|delay\\(|yield\\(" include src`
  should return no matches.
- Arduino examples remain under `examples/01_basic_bringup_cli` and continue to
  provide `Wire`, `millis()`, and `yield()` through example-local callbacks.
- IDF builds were not run in this environment because `idf.py` was not on PATH.

## Remaining Hardware Work

- Build `examples/esp_idf/basic` for ESP32-S3 and ESP32-S2 with ESP-IDF 6.0.1.
- Validate manufacturer ID `0x5449`, die ID `0x3220`, three-channel scaling,
  Mask/Enable clear behavior, alert APIs, and health/recovery behavior on
  hardware.
