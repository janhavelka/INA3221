# Prompt 04 — Conversion Timing and Readiness Model

Continue on branch:

```text
hardening/ina3221-industry-readiness
```

Start clean. If dirty, stop and report dirty files.

This chunk fixes conversion timing semantics.

## Spawn subagents

Use:

1. `timing-model-agent`
2. `device-specific-correctness-agent`
3. `test-fault-agent`
4. `docs-contract-agent`
5. `integration-review-agent`

## Background

The exploration report found that the current code likely multiplies scan time by averaging count when deciding when to poll CVRF. This can delay readiness checks by many seconds in high-averaging configurations.

The report says the datasheet clearly supports scan time based on:

```text
enabled channels × enabled signal conversion times
```

while averaging is response/filter behavior. Unless there is proof that CVRF latency equals scan time multiplied by averaging count, readiness must not wait that long.

## Audit first

Inspect:

- `getConversionTimeUs()`
- `getCycleTimeUs()`
- `_readConversionReadyAt()`
- `readBlocking()`
- `startConversion()`
- triggered-mode read readiness
- tests around `AVG_1024`

Search:

```bash
grep -R "getCycleTimeUs\|getConversionTimeUs\|AVG_1024\|conversionReady\|CVRF\|cycle" -n include src test README.md docs
```

## Required design

Split concepts.

### Scan time

Add or clarify:

```cpp
uint32_t getScanTimeUs() const;
```

Meaning:

```text
enabled channels × enabled measured signals × selected conversion time
```

Examples:

- 1 channel, shunt only, 140 us => 140 us.
- 3 channels, shunt+bus, 1.1 ms each => 6600 us.
- 3 channels, shunt+bus, 8.244 ms each => 49464 us.

### Averaged response time

Add or clarify:

```cpp
uint32_t getAveragedResponseTimeUs() const;
```

Meaning:

```text
scan time × averaging sample count
```

Document this as a response/filter-settling estimate, not necessarily CVRF-first-ready latency unless hardware validation proves it.

### Existing API compatibility

If `getCycleTimeUs()` already exists, keep it if practical. Either:

- make it alias `getScanTimeUs()` and document the behavior,
- keep old meaning but add deprecation/clarification,
- or add new APIs without breaking old one.

Pick the least disruptive clean approach, but avoid misleading names.

## Readiness polling

Update readiness/CVRF gating so it does not wait `scan_time * avg_count` before the first hardware check unless proof exists.

Better behavior:

- after single-shot trigger, do not poll before at least scan-time has elapsed,
- after scan-time, read CVRF using the safe/destructive Mask/Enable helper from Prompt 03,
- continue bounded polling until timeout.

Keep all waits bounded.

## Continuous mode

Document:

- continuous reads are latest-register reads,
- they are not guaranteed fresh since last call,
- freshness requires explicit conversion-ready/alert path, which is destructive for Mask/Enable flags.

## Tests required

Add tests named similarly to:

```text
test_scan_time_one_channel_shunt_only
test_scan_time_three_channels_shunt_bus
test_scan_time_all_conversion_time_enums
test_averaged_response_time_scales_with_avg
test_readiness_first_poll_uses_scan_time_not_avg_scaled_time
test_high_averaging_does_not_delay_first_cvrf_poll_for_seconds
test_blocking_timeout_remains_bounded
test_continuous_read_documented_as_latest_register_read
```

Update old tests that incorrectly asserted averaging-scaled readiness.

## Docs

Update README timing section, public header comments, latency/API table, and hardware validation matrix.

Include:

```markdown
| Concept | Formula | Used for |
| --- | --- | --- |
| Scan time | enabled channels × enabled signals × conversion time | earliest reasonable CVRF check |
| Averaged response estimate | scan time × averaging samples | filter/response/settling estimate |
```

Add hardware validation TODO:

- measure CVRF timing across AVG_1, AVG_4, AVG_16, AVG_64, AVG_256, AVG_1024,
- measure 1/2/3 channels,
- measure shunt-only, bus-only, shunt+bus,
- compare against scan-time and averaged-response estimates.

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
docs/INA3221_HARDENING_PHASE04_TIMING_MODEL.md
```

Include old behavior, new behavior, API changes, test changes, exact validation results, and hardware timing proof still required.

## Commit

```bash
git add include src test README.md docs examples tools
git commit -m "fix: clarify INA3221 scan timing and readiness polling"
git status --short
```

Stop after this chunk.
