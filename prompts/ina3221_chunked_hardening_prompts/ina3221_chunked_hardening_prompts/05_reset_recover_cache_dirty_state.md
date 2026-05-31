# Prompt 05 — Reset/Recover Cache Consistency and Dirty State

Continue on branch:

```text
hardening/ina3221-industry-readiness
```

Start clean. If dirty, stop and report dirty files.

This chunk fixes production state loss across reset/recover.

## Spawn subagents

Use:

1. `reset-recover-cache-agent`
2. `test-fault-agent`
3. `docs-contract-agent`
4. `integration-review-agent`

## Background

The exploration report found that alert and power-valid limit setters write hardware directly but do not cache the values. `softReset()` and `recover()` only restore config and Mask/Enable writable bits, so custom thresholds can silently revert to defaults.

This is a high-severity production issue.

## Audit writable registers

List all writable registers and classify them.

Expected classes:

1. Configuration register.
2. Critical alert limits per channel.
3. Warning alert limits per channel.
4. Shunt voltage sum limit.
5. Mask/Enable writable bits.
6. Power-valid upper limit.
7. Power-valid lower limit.
8. Any other documented writable register.

Create this table in the phase report:

```markdown
| Register | Address | Writable? | Cached? | Reapplied after reset? | Notes |
| --- | --- | --- | --- | --- | --- |
```

## Required implementation

### 1. Cache production writable settings

Add internal cached values and valid bits for:

- critical alert limit ch1/ch2/ch3,
- warning alert limit ch1/ch2/ch3,
- shunt voltage sum limit,
- power-valid upper limit,
- power-valid lower limit,
- Mask/Enable writable bits,
- any other production writable settings.

Use a compact struct if cleaner.

### 2. Update setters

Every successful setter for these registers must update cache.

On failed write, choose and document one strategy:

A. cache only confirmed hardware state, or  
B. cache desired state and mark hardware dirty if write fails.

For production clarity, strategy A is usually simpler, but either is acceptable if dirty state is explicit.

### 3. Add dirty/sync-needed diagnostics

Add `hardwareConfigDirty()`, `syncNeeded()`, or equivalent, plus an error/status field storing the status that caused dirty state.

Expose through `SettingsSnapshot`.

Dirty must be set when:

- a multi-register reapply fails partway,
- raw write touches cached writable settings,
- reset/recover changes unknown hardware state and not all cached state is reapplied.

Dirty must clear only after a full successful reapply/resync.

### 4. Reapply after reset/recover

Update:

- `softReset()`
- `recover()`
- internal config-apply paths

After reset/probe/config, reapply cached production writable settings.

Important:

- preserve original error on partial failure,
- mark dirty,
- do not claim recovery is complete if thresholds failed to reapply.

Add an explicit function if clean:

```cpp
Status applyCachedSettings();
Status resync();
```

### 5. Raw writes

Update `writeRegister16()` behavior:

- if raw write targets a cached writable register, mark dirty/sync-needed;
- if raw write targets read-only register, reject as Prompt 02 requested;
- document raw writes as diagnostic and cache-desynchronizing.

## Tests required

Add tests named similarly to:

```text
test_alert_limit_setters_update_cache
test_power_valid_setters_update_cache
test_shunt_sum_limit_setter_updates_cache
test_soft_reset_reapplies_cached_alert_limits
test_soft_reset_reapplies_cached_power_valid_limits
test_recover_reapplies_all_cached_writable_registers
test_recover_partial_failure_marks_dirty_and_preserves_error
test_dirty_clears_only_after_successful_resync
test_raw_write_to_cached_register_marks_dirty
test_raw_write_to_read_only_register_rejected
```

Use fake transport per-transaction failure injection. If current fake transport is not enough, extend it minimally here or leave deeper fake refactor for Prompt 07, but at least cover main reset/recover behavior.

## Docs

Update README, public header comments, SettingsSnapshot docs, raw register docs, and recover/reset docs.

Document:

- what is cached,
- what reset/recover reapply,
- what dirty/sync-needed means,
- how to clear dirty state,
- raw write consequences.

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
docs/INA3221_HARDENING_PHASE05_RESET_RECOVER_CACHE.md
```

Include writable register table, cache strategy chosen, dirty-state behavior, reset/recover behavior, tests added, validation results, and remaining limitations.

## Commit

```bash
git add include src test README.md docs examples tools
git commit -m "fix: preserve INA3221 writable settings across reset and recover"
git status --short
```

Stop after this chunk.
