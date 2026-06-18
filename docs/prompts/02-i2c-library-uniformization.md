# INA3221 I2C Uniformization Prompt

Repository: `INA3221`

Absolute path: `C:\Users\Honza\Documents\Projects\INA3221`

## Execution Rules

You are working inside this single repository. Implement this prompt directly;
do not repeat the cross-repository audit.

You may spawn subagents for read-only inspection of APIs, tests, I2C
transactions, docs, and diagnostics. Keep final judgment, edits, and
verification in the main agent.

Prefer simple, robust, readable code. Before adding code, inspect whether
existing code can be simplified, reused, tightened, or deleted.

Preserve dirty user changes. Do not commit unless explicitly asked.

## Common Uniformization Target

Apply this shared I2C library contract: injected non-owning transport, `Status` returns, cache-only `getSettings(SettingsSnapshot&) const`, active `probe()`/diagnostics named explicitly, `DriverState` with `state()` and `driverState()`, `isOnline()`, `lastOkMs()`, `lastErrorMs()`, `lastError()`, `consecutiveFailures()`, `totalFailures()`, and `totalSuccess()`.

Keep the common `Err` vocabulary append-only where missing: `OK`, `NOT_INITIALIZED`, `INVALID_CONFIG`, `INVALID_PARAM`, `I2C_ERROR`, `I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`, `I2C_BUS`, `DEVICE_NOT_FOUND`, `TIMEOUT`, `BUSY`, and `IN_PROGRESS`. Preserve INA3221-specific manufacturer/die ID and measurement readiness codes.

Uniformization is not a new base class or framework. Make only local, source-compatible additions and tests.

## Current State

- Public lifecycle and health are in `include\INA3221\INA3221.h`: `DriverState` at line 16, `probe()` at line 137, `recover()` at line 140, `getSettings(SettingsSnapshot&)` at line 143, `state()` at line 146, `lastOkMs()` through `totalSuccess()` at lines 153-158.
- Public register helpers are present as `readRegister16()` and `writeRegister16()` at `include\INA3221\INA3221.h:351-353`.
- Raw and tracked helpers are declared at `include\INA3221\INA3221.h:373-375`; health update is implemented in `src\INA3221.cpp:1492`.
- The driver has ID checks (`MANUFACTURER_ID_MISMATCH`, `DIE_ID_MISMATCH`) and a `softReset()` at `include\INA3221\INA3221.h:261`.
- No explicit HIL runner was found.
- Dirty hardware/cache diagnostics are less explicit than BME280, LDC1614, PCA9555, or MCP45HVX1.

## Best Sources To Adapt

- Use the `driverState()` alias style from `BME280\include\BME280\BME280.h:302-306`.
- Use BME280 dirty-state wording and tests as the register-mapped sensor reference: `BME280\include\BME280\BME280.h:133-134`, `:549-585`, and `BME280\src\BME280.cpp:1713-1725`.
- Use INA228 register-width documentation style where INA3221 register side effects are device-specific: `INA228\include\INA228\INA228.h:635-653`.

## Implementation Tasks

1. Add `DriverState driverState() const { return state(); }` beside `state()` in `include\INA3221\INA3221.h`.
   Preserve existing compatibility aliases; do not remove or rename public APIs to achieve uniform naming.
2. Audit all state-changing writes, including `writeRegister16()` and configuration/alert-limit helpers. If a failed write can leave hardware different from cache, add an explicit dirty/uncertain state using the local naming style. Prefer `hardwareConfigDirty()` only if it matches the existing snapshot vocabulary; otherwise add a narrow device-specific name.
3. If dirty state is added, include the root `Status` in `SettingsSnapshot`, expose a const accessor, and clear it only after successful `begin()`, `recover()`, or a verified full reapply path.
4. Keep `probe()` raw/no-health and identity-aware. Do not treat an ACK as manufacturer/die ID validation.
5. Audit every wait/poll path for finite timeout bounds and visible status returns. Normal measurement/register APIs must not hide retries; recovery remains explicit and application-scheduled.
6. Add HIL automation only if the repo has a maintained diagnostic CLI. If present, cover the common minimum contract: `version`, `scan`, `probe`, `settings`, `health`, failure-token classification, and dry-run/parser test support. Otherwise update docs to explicitly say HIL is not automated.

## API Changes Required

- Add `driverState()` alias.
- Add dirty-state accessors only if the audit in task 2 proves a real cache/hardware divergence risk.

## Simplifications Before Adding Code

- Before adding dirty state, check whether existing config writes can instead update cache only after all hardware writes succeed. Prefer that simpler path if it avoids a new public diagnostic.

## Tests To Add Or Update

- Native alias test for `driverState()`.
- Fault-injection tests for partial/failed config writes. Expected result: original transport error is returned and dirty/uncertain state is visible, or no cache divergence is possible.
- Probe failure must not change health counters.

## Commands To Run

- `pio test -e native`
- `pio run -e esp32s3dev`
- Optional HIL dry run only if implemented.

## Constraints And Non-Goals

- Do not invent a universal dirty-state framework.
- Do not add a bus owner, lock manager, or hidden retries inside the device driver.
- Preserve distinct timeout, address NACK/device-not-found, data NACK, bus, manufacturer-ID, die-ID, and measurement statuses. Do not collapse them into generic `I2C_ERROR` or use `DEVICE_NOT_FOUND` for timeout/data/bus failures.

## Risks And Open Questions

- Open: whether INA3221's current write paths already avoid cache divergence well enough to document "no dirty state needed" instead of adding a new API.
