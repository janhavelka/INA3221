# INA3221 ESP-IDF Port Implementation

Implemented after the ESP-IDF port branch was merged into `main`.

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
- `idf_component.yml` declares component-manager metadata for ESP-IDF 6.x and
  the ESP32-S2/S3 targets.
- `examples/esp_idf/basic` demonstrates application-owned bus/device setup with
  the new `driver/i2c_master.h` API, `esp_timer_get_time()` timing, and a
  FreeRTOS yield hook.
- The ESP-IDF entry point is a native fixed-buffer command shell with the same
  three-channel measurements, conversion controls, alert limits, raw-register
  diagnostics, INA3221 identity scanner, health/recovery, stress, and self-test
  workflows as the Arduino example.
- The ESP-IDF example does not include the Arduino CLI source and does not use
  Arduino compatibility facades.
- `tools/check_idf_example_contract.py` guards native IDF dependencies, native
  transport use, command parity, and the absence of Arduino framework tokens in
  IDF example code.

## Validation

- Static check target: `rg "<Arduino.h>|<Wire.h>|millis\\(|delay\\(|yield\\(" include src`
  should return no matches.
- Static parity checks:
  - `python tools/check_cli_contract.py`
  - `python tools/check_idf_example_contract.py`
  - `python tools/check_core_timing_guard.py`
- Arduino examples remain under `examples/01_basic_bringup_cli` and continue to
  provide `Wire`, `millis()`, and `yield()` through example-local callbacks.
- IDF builds were not run in this environment because `idf.py` was not on PATH.
- Arduino example behavior has owner hardware-test coverage and remains the
  reference behavior for this pass. ESP-IDF hardware validation has not been
  performed in this environment.

## Remaining Hardware Work

- Build `examples/esp_idf/basic` for ESP32-S3 and ESP32-S2 with ESP-IDF 6.0.1.
- Validate manufacturer ID `0x5449`, die ID `0x3220`, three-channel scaling,
  Mask/Enable clear behavior, alert APIs, and health/recovery behavior on
  hardware.
