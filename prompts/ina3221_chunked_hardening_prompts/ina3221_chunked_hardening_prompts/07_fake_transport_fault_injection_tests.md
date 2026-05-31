# Prompt 07 — Programmable Fake Transport and Fault-Injection Test Depth

Continue on branch:

```text
hardening/ina3221-industry-readiness
```

Start clean. If dirty, stop and report dirty files.

This chunk strengthens tests enough for production hardening.

## Spawn subagents

Use:

1. `test-fault-agent`
2. `device-specific-correctness-agent`
3. `integration-review-agent`

## Goal

The exploration report says the current fake bus is too simple. Upgrade the native fake transport to catch protocol, sequencing, read-clear, timeout, and partial-failure bugs.

Do not rewrite the entire test suite unnecessarily. Make the fake reusable and readable.

## Required fake transport capabilities

Add or refactor test helpers to support:

1. Transaction recording:
   - address,
   - register,
   - read/write,
   - tx bytes,
   - rx length,
   - timeout value,
   - sequence order.

2. Per-transaction failure injection:
   - fail next write,
   - fail next read,
   - fail on Nth transaction,
   - fail for specific register,
   - return specific `Status` code/detail.

3. Register behavior hooks:
   - Mask/Enable read-clear behavior,
   - software reset behavior,
   - ID register fixed values,
   - measurement register signed raw values,
   - writable-register model.

4. Address checking:
   - verify `0x40`-`0x43`,
   - reject invalid addresses,
   - preserve address NACK vs bus timeout if the status model supports it.

5. Timeout checking:
   - assert `i2cTimeoutMs` is passed to callbacks,
   - allow simulating timeout.

6. Side-effect assertions:
   - detect unexpected Mask/Enable read,
   - detect unexpected config write,
   - detect unexpected register write during TC preserve/diagnostic-only path.

## Tests to add or expand

### Identity/probe/status

```text
test_all_valid_addresses_accepted
test_invalid_addresses_rejected
test_probe_preserves_timeout_or_documents_mapping
test_probe_preserves_bus_error_or_documents_mapping
test_manufacturer_id_mismatch
test_die_id_mismatch
```

### Signed scaling/raw format

```text
test_shunt_negative_sign_extension
test_bus_negative_sign_extension_if_datasheet_signed_behavior_applies
test_reserved_low_bits_do_not_affect_scaled_conversion
test_raw_returns_register_word_not_shifted_counts
```

### Config matrix

```text
test_config_encode_decode_all_modes
test_config_encode_decode_all_averaging_values
test_config_encode_decode_all_conversion_times
test_config_rejects_invalid_all_channels_disabled_in_active_mode
```

### Alert/read-clear

```text
test_mask_enable_read_clear_behavior
test_internal_readiness_records_cleared_alerts
test_explicit_alert_read_records_reason
```

### Reset/recover partial failures

```text
test_recover_config_write_failure_preserves_error
test_recover_mask_enable_write_failure_sets_dirty
test_recover_limit_reapply_failure_sets_dirty
test_resync_clears_dirty_only_on_full_success
```

### Timing

```text
test_scan_time_formula_matrix
test_readiness_poll_gate_does_not_use_averaging_multiplier
test_blocking_timeout_no_timebase_behavior
```

### Register access

```text
test_raw_write_rejects_read_only_measurement_registers
test_raw_write_rejects_id_registers
test_raw_write_cached_register_marks_dirty
```

## Keep tests readable

If `test/test_basic.cpp` becomes too large, split into logical files if the test framework supports it:

```text
test/test_core_contracts.cpp
test/test_alert_side_effects.cpp
test/test_timing.cpp
test/test_reset_recover.cpp
test/test_fake_bus.h
```

Do not split if build-system complexity becomes a distraction.

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
docs/INA3221_HARDENING_PHASE07_FAULT_TESTS.md
```

Include fake transport capabilities added, test files changed, native test count before/after, exact validation results, and remaining hardware-only cases.

## Commit

```bash
git add test include src README.md docs tools
git commit -m "test: add INA3221 protocol fault injection coverage"
git status --short
```

Stop after this chunk.
