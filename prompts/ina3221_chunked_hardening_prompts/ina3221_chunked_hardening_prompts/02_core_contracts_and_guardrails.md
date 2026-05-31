# Prompt 02 — Core Contracts and Guardrails

Continue on branch:

```text
hardening/ina3221-industry-readiness
```

Start clean. If dirty, stop and report dirty files.

This chunk fixes non-device-specific production contracts before touching alert/timing logic.

## Spawn subagents

Use:

1. `core-contracts-agent`
2. `test-fault-agent`
3. `docs-contract-agent`
4. `integration-review-agent`

## Goals

Implement focused core hardening:

1. Delete accidental copy/move operations.
2. Split readable vs writable register validation.
3. Reject raw writes to read-only measurement and ID registers.
4. Add or document status-returning alternatives for lossy APIs.
5. Strengthen framework-leakage guard.
6. Document thread/ISR safety.

## 1. Delete copy/move operations

In `include/INA3221/INA3221.h`, explicitly delete copy and move construction/assignment for the `INA3221` driver object.

Add compile-time tests:

```cpp
static_assert(!std::is_copy_constructible_v<INA3221::INA3221>);
static_assert(!std::is_copy_assignable_v<INA3221::INA3221>);
static_assert(!std::is_move_constructible_v<INA3221::INA3221>);
static_assert(!std::is_move_assignable_v<INA3221::INA3221>);
```

Use the actual namespace/type names in the repo.

Reason: the object owns mutable config, health counters, offline state, cached conversion state, and I2C callback context.

## 2. Split readable vs writable register validation

Implement separate helpers:

```cpp
bool isReadableRegister(uint8_t reg);
bool isWritableRegister(uint8_t reg);
```

Readable registers should include documented readable measurement, config, mask/enable, limit, manufacturer ID, and die ID registers.

Writable registers should include only documented writable registers, such as configuration, critical/warning limits, shunt-sum limit, Mask/Enable writable bits, and power-valid limits.

Update `writeRegister16()` to reject measurement and ID registers with the repo’s best matching invalid-parameter status.

Do not add an unsafe raw write unless examples require it. If you do add it, name it clearly, for example `writeRegister16Unsafe()`.

## 3. Lossy APIs

Audit:

- `tick()`
- `end()`
- `conversionReady()`
- invalid-channel getters such as `getChannelEnable()` and `getShuntResistance()`

Do not break APIs unnecessarily. Add status-returning alternatives only if clean. Otherwise document these as lossy convenience methods.

Preferred additions if simple:

- `Status tickStatus(uint32_t nowMs)` with `void tick()` wrapper.
- `Status shutdown()` if `end()` is best-effort.
- status-returning invalid-channel getters if current getters return sentinel values.

## 4. Strengthen framework-leakage guard

Update or add a guard script so `include/` and `src/` reject these core tokens:

```text
Arduino.h
Wire.h
TwoWire
Serial
String
driver/
freertos/
esp_
vTaskDelay
delay(
millis(
micros(
new
malloc
std::vector
std::string
```

Avoid false positives in docs/comments if necessary, but core code must stay clean.

## 5. Docs

Update README and public Doxygen comments to state:

- driver instances are not internally thread-safe,
- external serialization is required on shared buses,
- I2C/blocking APIs are not ISR-safe,
- callbacks must not call back into the same instance,
- raw register writes are diagnostic/advanced and can desync cached state,
- convenience APIs may collapse errors.

## Tests

Add tests for:

- copy/move deletion,
- read-only raw write rejection,
- readable registers still readable,
- invalid raw register still rejected,
- lossy convenience APIs documented or status alternatives work,
- guard script catches forbidden core tokens.

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
```

## Output report

Create:

```text
docs/INA3221_HARDENING_PHASE02_CORE_CONTRACTS.md
```

Include what changed, public API changes, migration notes, tests added, validation results, and anything deferred.

## Commit

```bash
git add include src test tools README.md docs
git commit -m "fix: harden INA3221 core contracts"
git status --short
```

Stop after this chunk.
