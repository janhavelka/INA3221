# Prompt 08 — Docs, Examples, ESP-IDF/Arduino Honesty, and Hardware Validation Matrix

Continue on branch:

```text
hardening/ina3221-industry-readiness
```

Start clean. If dirty, stop and report dirty files.

This chunk polishes user-facing contracts and example honesty after the core behavior is fixed.

## Spawn subagents

Use:

1. `docs-examples-idf-agent`
2. `cli-contract-agent`
3. `integration-review-agent`

## Goals

1. Make README and public docs production-honest.
2. Update Arduino and ESP-IDF examples so they are clearly diagnostic unless they implement production bus ownership/locking.
3. Ensure CLI exposes useful diagnostics for new side-effect/dirty/timing contracts.
4. Add a concrete hardware validation matrix.
5. Add or prepare pure ESP-IDF build validation.

## README must include

Add or strengthen sections for:

- Quick start with explicit shunt-resistance configuration.
- Physical safety:
  - 26 V max bus sense input,
  - shunt differential limits,
  - open-drain alert outputs and pull-ups/level shifting,
  - Kelvin routing,
  - register scale does not override physical maximums.
- Framework-neutral core.
- I2C ownership/injected transport.
- Status/error model.
- Timing:
  - scan time,
  - averaged response estimate,
  - blocking timeout behavior,
  - continuous latest-sample semantics.
- Mask/Enable read-clear side effects:
  - exactly which APIs are destructive,
  - how to retrieve internally-cleared snapshot.
- Reset/recover:
  - what settings are cached,
  - what gets reapplied,
  - dirty/sync-needed state.
- PV:
  - supported helper contract,
  - raw register contract,
  - topology limitations.
- TC:
  - supported lifecycle or diagnostic-only status.
- Summation:
  - equal-shunt requirement for current summation.
- Thread/ISR safety.
- Hardware validation matrix.
- Release-readiness status.

## Public headers

Update Doxygen comments for every API touched by prior chunks.

Important APIs:

- `begin()`
- `tick()` / status alternative
- `readAlertFlags()`
- `readConversionReady()`
- `conversionReady()`
- `readBlocking()`
- raw register read/write
- alert/PV limit setters
- summation setters
- reset/recover/resync
- timing APIs
- snapshot/dirty APIs

## CLI/example polish

Audit Arduino CLI and ESP-IDF CLI.

Add commands only if useful and small:

- print timing model,
- print dirty/sync-needed state,
- print last internally-cleared alert snapshot,
- print active shunt values,
- print PV/TC support status,
- print warning when current/power reads use default shunt values.

Do not turn examples into large apps.

Mark examples clearly:

- diagnostic bring-up example,
- single-owner/single-task assumptions,
- not a production shared-bus manager unless it actually has bus/driver locking and ownership policy.

## ESP-IDF example

If the current IDF example is single-task/single-owner, document that honestly.

If adding a mutex is small and clean, add it around bus access in the IDF transport/example. Do not move locking into the core unless explicitly designed.

Try to run:

```bash
idf.py --version
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

If `idf.py` is unavailable, do not claim success.

## Hardware validation matrix

Create or update:

```text
docs/INA3221_HARDWARE_VALIDATION_MATRIX.md
```

Include at minimum:

| Area | Required validation |
| --- | --- |
| Addresses | 0x40, 0x41, 0x42, 0x43 |
| Identity | Manufacturer ID 0x5449, die ID 0x3220 |
| Channels | all three shunt and bus channels |
| Shunt voltage | positive and safe negative if possible |
| Bus voltage | representative safe rails, never >26 V |
| Single-shot | trigger, CVRF, retrigger |
| Continuous | update rate and latest-read behavior |
| Averaging | CVRF timing for representative averaging values |
| Alerts | critical, warning, sum, latch/non-latch |
| Power-valid | PV upper/lower and topology |
| TC | only if supported |
| Faults | NACK, timeout, unplug/replug, brownout |
| Recovery | software reset, recover, resync |
| Soak | long read/stress run |
| Frameworks | Arduino S2/S3 and native IDF S2/S3 |

Add command suggestions for the existing CLI if applicable.

## Validation

Run:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python -m platformio pkg pack
```

If available:

```bash
idf.py --version
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

## Output report

Create:

```text
docs/INA3221_HARDENING_PHASE08_DOCS_EXAMPLES_VALIDATION.md
```

Include docs updated, CLI/example changes, IDF build status, hardware validation matrix path, tests run, and remaining hardware validation gaps.

## Commit

```bash
git add README.md docs include examples tools test platformio.ini CMakeLists.txt idf_component.yml .github
git commit -m "docs: document INA3221 production contracts and validation matrix"
git status --short
```

Stop after this chunk.
