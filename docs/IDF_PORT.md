# Native ESP-IDF integration

[Documentation index](README.md) · [Project README](../README.md)

The repository is an ESP-IDF component and its core remains framework-neutral.
The production API is the same under Arduino and ESP-IDF: an application-owned
transport, a complete `DeviceProfile`, and a cooperative job engine. The native
example targets ESP-IDF 6.0.1 and uses the new I2C master driver.
This native target is independent of ESP-IDF `5.5.5` bundled with the
pioarduino/Arduino-ESP32 example build.

Release metadata is updated atomically by the release owner, with
`library.json` as the source of truth.

## Component boundary

The root `CMakeLists.txt` registers `src/INA3221.cpp` and exports `include/`.
Core headers and source do not include ESP-IDF, FreeRTOS, Arduino, or Wire
headers. `idf_component.yml` declares ESP-IDF component metadata and supported
targets.

The example-local adapter is under
`examples/esp_idf/basic/main/Ina3221IdfI2cTransport.*`. Applications should
normally own an equivalent context alongside their central I2C manager instead
of copying the example singleton.

## Ownership contract

Only the application bus owner calls the driver. It must serialize all API
calls and callbacks in one non-ISR context. The object is non-copyable,
non-movable, non-reentrant, and not ISR-safe. It creates no task or lock and
does not configure pins, clock, pull-ups, or controller timeouts.

Each `I2cWriteFn` callback is exactly one physical transmit attempt. Each
`I2cWriteReadFn` callback is exactly one non-interleaved pointer-write/read
attempt. Success means the requested lengths completed. Callbacks must not
retry, recover, reconfigure the bus, interleave a second device, or call back
into the INA3221 object.

The native adapter:

- uses `i2c_master_transmit()` once for a write;
- uses `i2c_master_transmit_receive()` once for a register read;
- passes the supplied finite millisecond timeout to IDF after rejecting values
  outside the signed IDF range;
- maps `ESP_ERR_TIMEOUT` to `I2C_TIMEOUT` and retains `esp_err_t` in
  `Status::detail`;
- maps other bus failures without hidden retries or recovery.

`ESP_ERR_INVALID_RESPONSE` does not reliably identify the NACK phase of a
combined transfer, so the example maps it to a general I2C error and preserves
the native detail.

## Production flow

1. The application creates and owns the IDF bus/device handles.
2. Fill `TransportConfig` with the single-attempt callbacks, context pointer,
   and default transfer timeout.
3. Fill the complete `DeviceProfile`, including a nonzero fixed-unit shunt
   calibration for every enabled channel.
4. Call `bind()`. It validates and stores the contracts without I2C.
5. Call `startInitialize(nonzeroRequestId, absoluteDeadlineMs)`.
6. From the bus-owner loop, call `pollJob()` with a current monotonic time,
   effective deadline, finite timeout, and normally `maxTransfers = 1`.
7. Inspect cache-only `JobProgress`. When `resultPending` is true, call
   `takeJobResult()` exactly once before starting another job.
8. Use `startTriggeredSample()`, `startContinuousSample()`,
   `startApplyProfile()`, `startReconcile()`, or `startPowerDown()` under the
   same admission policy.

`cancelJob()` and `unbind()` are bus-silent. A confirmed/ambiguous write can
leave configuration certainty `DIRTY`/`UNKNOWN`; inspect the terminal
`HardwareEffect` and reconcile before measurement admission. Health state is
passive telemetry and never suppresses an owner-admitted transfer.

The authoritative successful-path callback maxima, triggered-conversion wake
margin, and deadline admission rules are maintained in the root README's
[deterministic-bounds section](../README.md#deterministic-bounds). Budget one
limits each normal service call to one synchronous backend callback.

## Native example

`examples/esp_idf/basic` provides:

- `app_main` and the new `driver/i2c_master.h` API;
- `esp_timer_get_time()` monotonic timestamps;
- FreeRTOS bounded waits/yields;
- a fixed 128-byte C command buffer and no Arduino compatibility facade;
- zero-I2C bind, budget-one staged initialization, triggered fixed-unit sample,
  progress, cancellation, and take-once result handling;
- the existing scan, measurement, configuration, alert, raw register, health,
  stress, and self-test diagnostics.

The CLI commands `job`, `job sample`, and `job cancel` demonstrate progress,
triggered admission, and bus-silent cancellation. The transport callback does
not implement retry or bus recovery; those remain application policy.

## ESP-IDF 6.0.1 build

From the repository root:

```bash
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

Do not use the removed command-link driver (`driver/i2c.h`,
`i2c_cmd_link_*`, or `i2c_driver_install`). The example component declares the
split IDF driver dependencies it uses.

## Validation meaning

Static checks verify source contracts only:

```bash
python tools/check_idf_example_contract.py
python tools/check_core_timing_guard.py
python tools/check_metadata_consistency.py
```

A passing static check does not compile ESP-IDF headers, link the component, or
prove either chip target. Local compile success may be claimed only from a real
`idf.py` transcript. If `idf.py` is unavailable locally, record both target
builds as `NOT RUN`.

CI has a separate compiled-build job using the official ESP-IDF 6.0.1 container
and runs sequential `idf.py` builds for ESP32-S3 and ESP32-S2. Only a successful
job log proves those CI builds for a revision; workflow configuration by itself
is not a passing result.

Hardware validation is separate again. It must verify identity, scaling,
conversion-ready/alert read-clear behavior, profile reconciliation, transport
fault reporting, and owner-level recovery on a real fixture. INA3221 has no
EEPROM/NVM, so rare program/erase latency is not applicable.

Official references:

- [ESP-IDF 6.0.1 I2C master driver](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32/api-reference/peripherals/i2c.html)
- [ESP-IDF 6.0 peripheral migration guide](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32c6/migration-guides/release-6.x/6.0/peripherals.html)
