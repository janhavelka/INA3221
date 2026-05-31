# Prompt 03 — Mask/Enable Read-Clear and Alert Side Effects

Continue on branch:

```text
hardening/ina3221-industry-readiness
```

Start clean. If dirty, stop and report dirty files.

This chunk fixes the most important production blocker: internal reads of `REG_MASK_ENABLE` can clear CVRF and alert flags.

## Spawn subagents

Use:

1. `alert-side-effects-agent`
2. `test-fault-agent`
3. `docs-contract-agent`
4. `integration-review-agent`

## Background

The exploration report found that:

- `_readConversionReadyAt()` reads `REG_MASK_ENABLE`.
- `tick()`, `readConversionReady()`, `conversionReady()`, triggered measurement reads, and `readBlocking()` can enter this path.
- INA3221 Mask/Enable reads clear CVRF and several alert flags.
- The driver can silently erase field fault evidence before the application calls diagnostics.

This must become explicit, safe, and test-covered.

## Audit first

Search for every direct/indirect Mask/Enable read:

```bash
grep -R "REG_MASK_ENABLE\|MASK_ENABLE\|readConversionReady\|conversionReady\|_readConversionReady\|tick(" -n include src test examples docs README.md
```

Map every path:

- explicit user alert read,
- explicit user conversion-ready read,
- internal readiness polling,
- triggered-mode measurement read,
- blocking read,
- tick/background path,
- CLI paths.

Write this map into the phase report.

## Required design

Create a single internal helper for destructive Mask/Enable reads.

Suggested shape:

```cpp
Status _readMaskEnableDestructive(uint16_t& raw,
                                  MaskEnableReadReason reason,
                                  bool trackHealth);
```

Use repo naming/style.

It must:

1. Read `REG_MASK_ENABLE`.
2. Treat the read as destructive/read-clear.
3. Capture the raw value.
4. Extract CVRF and alert bits.
5. Store a diagnostic snapshot of flags cleared by this read.
6. Store the source/reason:
   - explicit alert read,
   - explicit conversion-ready read,
   - internal tick,
   - internal blocking/readiness wait,
   - triggered measurement readiness,
   - raw register read if applicable.
7. Preserve status errors precisely.

Add a public diagnostic API or extend `SettingsSnapshot`.

Possible contract:

```cpp
struct AlertSnapshot {
    uint16_t rawMaskEnable;
    AlertFlags flags;
    bool valid;
    bool clearedByDriver;
    uint32_t timestampMs;
    MaskEnableReadReason reason;
};

Status getLastClearedAlertSnapshot(AlertSnapshot& out) const;
void clearLastClearedAlertSnapshot();
```

Use a simpler design if it fits the repo better.

## API behavior

Make behavior explicit:

- `readAlertFlags()` is a destructive explicit read.
- `readConversionReady()` is destructive because it reads Mask/Enable.
- `conversionReady()` is a lossy convenience wrapper if kept.
- `tick()` must not silently lose flags. Either avoid Mask/Enable reads when time-only readiness is enough, or update the cleared-flag snapshot.
- Triggered measurement readiness must not silently lose flags.
- `readBlocking()` must document whether it can clear flags.

## Fake bus behavior for this chunk

Extend fake I2C so reading `REG_MASK_ENABLE` clears hardware read-clear bits.

Behavior:

1. Fake register contains CVRF/alert bits.
2. First read returns bits.
3. After read, clear read-clear bits.
4. Fake can tell whether a driver-internal read happened.

## Tests required

Add tests named similarly to:

```text
test_read_alert_flags_destructive_read_reports_flags
test_mask_enable_second_read_clears_read_clear_bits
test_read_conversion_ready_records_cleared_alert_snapshot
test_tick_does_not_silently_discard_mask_enable_flags
test_triggered_read_records_internal_mask_enable_clear
test_read_blocking_records_internal_mask_enable_clear
test_mask_enable_read_failure_preserves_status
```

Also test that normal latest continuous reads do not read Mask/Enable if they should not.

## Docs

Update README, public header Doxygen, IDF docs if relevant, and CLI help if user-visible behavior changes.

Add a dedicated section:

```markdown
## INA3221 alert/readiness side effects

Reading Mask/Enable is destructive for CVRF and alert flags...
```

Document exactly which APIs are destructive.

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
docs/INA3221_HARDENING_PHASE03_ALERT_SIDE_EFFECTS.md
```

Include all paths that read Mask/Enable, final side-effect contract, public API changes, tests added, validation results, and remaining alert/SMBus-alert limitations.

## Commit

```bash
git add include src test README.md docs examples tools
git commit -m "fix: preserve INA3221 alert evidence across readiness reads"
git status --short
```

Stop after this chunk.
