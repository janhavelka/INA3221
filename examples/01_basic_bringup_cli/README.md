# Arduino/PlatformIO Bring-up CLI

[Back to project README](../../README.md)

This example is a bring-up and diagnostic application for ESP32-S2/S3 using
Arduino under PlatformIO. It demonstrates the production cooperative owner API
at startup and exposes both production-job and legacy diagnostic commands.

The helper files under `examples/common/` belong to the example only. They are
not installed library code and should not become transport or board policy in
the driver core.

## Configure hardware

Review `examples/common/BoardConfig.h` before building. Its defaults are
reference values, not universal board assignments:

| Setting | Example default |
|---|---:|
| SDA | GPIO 8 |
| SCL | GPIO 9 |
| I2C frequency | 400 kHz |
| Transfer timeout | 50 ms |
| INA3221 address | `0x40` |
| Optional ESP32-S3 LED | GPIO 48 |

Confirm voltage levels, pull-ups, A0 address selection, shunt values, and load
limits on the actual fixture before connecting it. Change only the example
configuration for board wiring; the library must not own pins or bus setup.

## Build and run

From the repository root:

```bash
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s3dev -t upload
python -m platformio device monitor -e esp32s3dev
```

Use `esp32s2dev` in the same commands for the ESP32-S2 target. The monitor is
configured for 115200 baud and LF line endings in the root `platformio.ini`.

## Startup flow

The application:

1. configures and scans the application-owned I2C bus;
2. builds `TransportConfig` and a complete triggered `DeviceProfile`;
3. calls bus-silent `bind()`;
4. starts identity/profile initialization with an absolute deadline;
5. services at most one physical callback per owner-loop pass;
6. takes the terminal initialization result; and
7. starts and consumes a fixed-unit triggered sample.

The transport adapter performs one bounded transaction attempt per callback;
the combined-read callback keeps its pointer write/read non-interleaved. Retry,
recovery, scheduling, and admission remain application policy.

## CLI use

Type `help` or `?` for the authoritative command list. Particularly useful
production-flow commands are:

| Command | Purpose |
|---|---|
| `job` | Show current cooperative progress/result state |
| `job sample` | Admit a budget-one triggered sample |
| `job cancel` | Cancel the active job without I2C |
| `settings` / `drv` | Show cached configuration and health |
| `probe` | Perform raw identity diagnostics |
| `selftest` | Run bounded safe command checks |
| `stress [N]` | Run bounded measurement cycles |
| `stress_mix [N]` | Exercise a bounded mixed-operation sequence |

Raw writes and configuration mutation are diagnostic tools. They can make the
managed profile `DIRTY` or `UNKNOWN`; do not treat the result as production
state until it has been reconciled and verified.

## Validation boundary

A successful build proves compilation for the selected Arduino target. It does
not prove INA3221 identity, scaling, alert wiring, shunt safety, or recovery on
the connected fixture. Use the CLI and a controlled load for those checks, and
retain the exact target/configuration with any HIL evidence.
