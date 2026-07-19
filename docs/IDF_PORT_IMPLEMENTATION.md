# Native ESP-IDF implementation status

Last updated: 2026-07-19

## Implemented boundary

- The root is an ESP-IDF component; core `include/` and `src/` remain free of
  framework headers and platform calls.
- `TransportConfig` carries non-owning I2C callbacks and optional compatibility
  time/yield hooks. `DeviceProfile` carries the complete fixed-size desired
  device state and calibration.
- `bind()`/`unbind()` are zero-I2C/bus-silent. The cooperative engine covers
  initialize, apply, reconcile, triggered/continuous sample, and verified
  power-down jobs.
- `PollContext` carries absolute monotonic time, the effective owner deadline,
  a per-transfer timeout, and a strict callback budget. The examples use budget
  one.
- Terminal state and hardware effect are retained in a take-once `JobResult`.
  Samples use fixed slots/units plus request/profile provenance. Destructive
  alert events are retained until explicitly taken.
- Applied measurement and alert certainty are explicit. Passive
  READY/DEGRADED/OFFLINE health remains diagnostic and is not an admission gate.

## ESP-IDF example files

- `examples/esp_idf/basic/main/main.cpp` defines native `app_main`, owns the
  example bus, drives staged initialization and a triggered sample, and retains
  the full fixed-buffer CLI.
- `Ina3221IdfI2cTransport.cpp` uses one
  `i2c_master_transmit()`/`i2c_master_transmit_receive()` call per callback,
  rejects non-finite or out-of-range timeouts, and performs no retry/recovery.
- The example uses `esp_timer`, FreeRTOS waits/yields, and
  `driver/i2c_master.h`; it contains no Arduino CLI source, `Arduino.h`, `Wire`,
  `String`, `Serial`, `TwoWire`, or compatibility facade.
- `tools/check_idf_example_contract.py` guards those structural and CLI
  contracts.

## Validation status and evidence rules

The following are static source checks, not ESP-IDF compiles:

```bash
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python tools/check_core_timing_guard.py
python tools/check_metadata_consistency.py
```

This development environment did not provide local `idf.py`; local ESP32-S3
and ESP32-S2 IDF builds must therefore be reported as `NOT RUN` unless a later
real transcript is attached.

CI is configured to run the actual compiler and linker in the official
`espressif/idf:v6.0.1` container, first for ESP32-S3 and then ESP32-S2. A green
CI job log is compiled-build evidence for its exact revision. Merely passing
the static guard or reading the workflow is not equivalent evidence.

PlatformIO Arduino builds and native host tests are separate regression gates.
Hardware validation is also separate and must not be inferred from any build.
Remaining fixture work includes identity, three-channel fixed-unit scaling,
maximum conversion timing, alert retention/read-clear behavior, deadline and
cancellation behavior, ambiguous writes, and application-owned recovery.

INA3221 registers are volatile and the device has no EEPROM/NVM. There is no
rare persistence-operation latency to characterize.
