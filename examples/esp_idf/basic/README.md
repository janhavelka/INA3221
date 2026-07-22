# Native ESP-IDF Bring-up CLI

[Back to project README](../../../README.md) ·
[Native integration guide](../../../docs/IDF_PORT.md)

This is a native ESP-IDF application. It uses `app_main`, the ESP-IDF 6.x
`driver/i2c_master.h` API, `esp_timer`, FreeRTOS waits, and fixed C buffers. It
does not include Arduino, Wire, `String`, `Serial`, or a compatibility facade.

## Configure hardware

The example-local defaults are declared near the top of `main/main.cpp`:

| Setting | Example default |
|---|---:|
| SDA | GPIO 8 |
| SCL | GPIO 9 |
| I2C frequency | 400 kHz |
| Transfer timeout | 50 ms |
| INA3221 address | `0x40` |
| CLI line buffer | 128 bytes |

Adjust them for the actual board. Confirm A0 selection, pull-ups, voltage
levels, shunt values, and load limits before connecting a fixture.

## Build and flash

From the repository root:

```bash
idf.py -C examples/esp_idf/basic set-target esp32s3
idf.py -C examples/esp_idf/basic build
idf.py -C examples/esp_idf/basic flash monitor
```

Replace `esp32s3` with `esp32s2` for that target. Running `set-target` rewrites
the example's generated build configuration; do not infer that the other target
still has current build evidence after switching.

The root repository is loaded as an extra component by the example's
`CMakeLists.txt`. The example component explicitly depends on the INA3221 core,
I2C/GPIO drivers, timer, and FreeRTOS.

## Demonstrated ownership model

`main/Ina3221IdfI2cTransport.*` is an example-local adapter. Each library
callback maps to one `i2c_master_transmit()` or
`i2c_master_transmit_receive()` attempt using the supplied timeout. It preserves
`esp_err_t` in `Status::detail` and performs no hidden retry or recovery.

The application owns bus/device handles and drives zero-I2C bind, budget-one
initialization, triggered fixed-unit sampling, progress inspection, bus-silent
cancellation, and take-once results from one serialized context. Reusable
applications should own their own adapter context beside the central I2C
manager instead of copying the example singleton.

Type `help` in the monitor for the authoritative CLI command list. The `job`,
`job sample`, and `job cancel` commands expose the cooperative flow; scan,
measurement, alert, raw-register, health, stress, and self-test diagnostics are
also available.

## Validation boundary

The repository's static IDF contract check does not compile ESP-IDF:

```bash
python tools/check_idf_example_contract.py
```

Only a successful `idf.py build` transcript proves compiler/linker success for
the selected target and exact revision. Hardware behavior is a separate HIL
claim. See the [integration guide](../../../docs/IDF_PORT.md) for detailed
ownership, timeout mapping, and evidence rules.
