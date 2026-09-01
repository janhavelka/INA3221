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

The `esp32s3dev` environment additionally assumes 4 MB QIO flash, 2 MB QSPI
PSRAM, and USB Serial/JTAG HWCDC. Adjust or remove the flash/PSRAM settings and
`BOARD_HAS_PSRAM` for a different module; a successful compile does not prove
that the selected memory geometry matches the board.

Confirm voltage levels, pull-ups, A0 address selection, shunt values, and load
limits on the actual fixture before connecting it. Change only the example
configuration for board wiring; the library must not own pins or bus setup.

## Build and run

From the repository root:

```bash
.\scripts\pio.cmd run -e esp32s3dev
.\scripts\pio.cmd run -e esp32s3dev -t upload
.\scripts\pio.cmd device monitor -e esp32s3dev
```

Use `esp32s2dev` in the same commands for the ESP32-S2 target. The monitor is
configured for 115200 baud and LF line endings in the root `platformio.ini`.
The S3 environment uses USB Serial/JTAG HWCDC, so upload and monitoring retain
one COM-port identity with DTR and RTS deasserted.

The repository exact-pins pioarduino `55.03.311`, Arduino-ESP32 `3.3.11`, and
the Arduino core's ESP-IDF `5.5.5` libraries.

For a strict automated qualification on an isolated fixture, follow the
[HIL guide](../../docs/HIL.md). A typical bounded invocation is:

```bash
python tools/hil_cli_runner.py --port COM5 `
  --stress-count 500 --stress-mix-count 500 `
  --sample-benchmark --benchmark-count 500
```

Replace `COM5` with the fixture port. The runner leaves DTR/RTS deasserted by
default and asserts exact stress totals and final health. When a soak is
requested, `--soak-failure-limit 1` stops it on the first failed command.

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

ESP32 `TwoWire` exposes a fixed bus-level timeout rather than a tighter timeout
for each callback. Every Arduino owner poll path applies the same admission
rule: automatic service and the `stress_owner` sampler pause bus traffic during
the last fixed-timeout window before a deadline, while `job step` also caps an
oversized budget to `remainingMs / configuredTimeoutMs` and prints the
adjustment. This prevents the driver from requesting a callback bound the
backend cannot honor; a zero safe budget is a deliberate bus-silent poll.

## CLI use

Type `help` or `?` for the authoritative command list. Particularly useful
production-flow commands are:

| Command | Purpose |
|---|---|
| `job progress` / `job result` | Show full cooperative progress or the retained terminal result |
| `job init` / `job apply` / `job reconcile` | Admit lifecycle/profile jobs |
| `job sample` / `job continuous [0\|1]` | Admit triggered or continuous fixed-unit samples |
| `job powerdown` | Admit verified power-down |
| `job step <0..255>` / `job auto <0\|1>` | Select an exact manual transfer budget or loop servicing |
| `job cancel` | Cancel the active job without I2C |
| `cancel` | Abandon an outstanding legacy conversion without I2C |
| `job lastsample` / `job alerts [take]` | Inspect retained sample and alert evidence |
| `scanina` / `addr` / `init` / `end` | Discover, select, bind, initialize, and unbind addresses `0x40`-`0x43` |
| `freq [10000..400000]` | Inspect or change the application-owned I2C clock with identity verification and rollback |
| `profile` / `direction` | Inspect the complete desired profile or set host current polarity |
| `settings` / `drv` / `diag` | Show configuration certainty, complete health, and cache-only aggregate diagnostics |
| `verify` / `mismatch` | Capture all 11 managed registers or replay exact retained mismatch evidence |
| `probe` | Perform raw identity diagnostics |
| `selftest` | Run bounded safe command checks |
| `stress [N]` | Run bounded measurement cycles |
| `stress_mix [N]` | Exercise a bounded mixed-operation sequence |
| `stress_owner [N]` / `stress_freq [N]` | Stress cooperative jobs or alternate 100/400 kHz with ID checks |
| `hilrun` / `hilmark` / `xfer_*` | Provide framed automation and exact physical-transfer evidence |

Raw writes and configuration mutation are diagnostic tools. They can make the
managed profile `DIRTY` or `UNKNOWN`; do not treat the result as production
state until it has been reconciled and verified.

## Validation boundary

A successful build proves compilation for the selected Arduino target. It does
not prove INA3221 identity, scaling, alert wiring, shunt safety, or recovery on
the connected fixture. Use the CLI and a controlled load for those checks, and
retain the exact target/configuration with any HIL evidence. The automated
coverage and explicit non-claims are maintained in the [HIL guide](../../docs/HIL.md).
