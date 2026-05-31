# Prompt 06 — Power-Valid, Timing-Control, Summation, and Safety Contracts

Continue on branch:

```text
hardening/ina3221-industry-readiness
```

Start clean. If dirty, stop and report dirty files.

This chunk makes INA3221 feature contracts honest and safe.

## Spawn subagents

Use:

1. `pv-tc-summation-agent`
2. `device-specific-correctness-agent`
3. `docs-contract-agent`
4. `test-fault-agent`
5. `integration-review-agent`

## Background

The exploration report found:

- Power-valid support is raw and incomplete.
- Timing-control alert sequencing can be disabled by normal `begin()`.
- Summation lacks equal-shunt warnings/guards.
- Default shunt values can produce plausible but wrong current.
- README lacks prominent 26 V physical bus-input safety warnings.

## 1. Power-valid contract

Power-valid requires bus-voltage measurement and topology constraints. Do not pretend raw register writes are a safe PV workflow.

Keep raw PV setters if they exist, but label them raw/advanced.

Add safe helpers if clean, for example:

```cpp
Status configurePowerValidLimits(float lowerVolts, float upperVolts);
Status validatePowerValidConfiguration() const;
```

The safe helper must reject or warn via status when:

- mode does not include bus voltage measurement,
- required channels are disabled,
- topology is incompatible or unknown,
- voltage is outside safe/representable range.

Do not invent support for two-channel PV topology if the datasheet requires all three channels. Document unused-channel wiring requirements.

Tests:

- safe PV helper rejects shunt-only mode,
- safe PV helper rejects disabled-channel topology if PV requires all channels,
- raw PV setters still mask reserved bits,
- PV default/reset behavior remains tested,
- PV helper encodes volts correctly if implemented.

## 2. Timing-control alert contract

The report says writing Configuration before timing-control sequencing completes disables TC until power cycle or software reset.

Choose one honest path.

### Option A — TC supported

Implement a config option such as:

```cpp
Config::preserveTimingControlSequence
```

In this mode:

- `begin()` must avoid the early Configuration write that disables TC,
- user must explicitly release/complete TC sequence,
- docs/tests must prove behavior.

### Option B — TC diagnostic-only for now

Document that:

- normal `begin()` writes Configuration,
- this disables timing-control sequencing,
- `timingControl` flag is decoded only for diagnostic visibility,
- production TC support is not implemented yet.

Unless the code already has a clean TC lifecycle, choose Option B to avoid overengineering.

## 3. Summation equal-shunt contract

The shunt-voltage sum alert is only meaningful for current summation if included channels use equal shunt resistors.

Required:

- Add README and header warning.
- Add safe helper if clean:

```cpp
Status setSummationChannelsChecked(bool ch1, bool ch2, bool ch3);
```

This helper should reject unequal configured shunt resistances unless an explicit override is passed.

Keep raw `setSummationChannels()` if it exists, but label it raw/advanced.

Tests:

- checked helper accepts equal shunts,
- checked helper rejects unequal shunts,
- raw helper still allows advanced use if kept,
- docs mention voltage-sum vs current-sum distinction.

## 4. Default shunt safety

The default `0.1 ohm` can produce plausible wrong values.

Choose a clean contract:

Option A:
- Require explicit shunt values before current/power APIs return success.

Option B:
- Keep defaults but add strong warnings in README, examples, and public headers.

Option C:
- Add `Config::requireExplicitShuntsForCurrent`, defaulting to false for compatibility but recommended true.

Do not break the API unless justified. At minimum, strengthen docs and examples.

## 5. Physical safety documentation

Add prominent safety documentation:

- INA3221 bus voltage input physical range is 0 V to 26 V.
- Register scale can represent more than safe physical input; this does not permit applying >26 V.
- Shunt differential and common-mode limits must be respected.
- Warning/Critical/PV outputs are open-drain style system signals requiring correct pull-up/level handling.
- Use Kelvin routing for shunts.
- Current/power calculations are only as good as shunt value/tolerance/layout.

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
docs/INA3221_HARDENING_PHASE06_PV_TC_SUMMATION.md
```

Include PV contract chosen, TC support level chosen, summation contract, shunt default decision, safety docs added, tests and validation, and deferred hardware validation.

## Commit

```bash
git add include src test README.md docs examples tools
git commit -m "docs: clarify INA3221 PV TC summation and safety contracts"
git status --short
```

Stop after this chunk.
