# Prompt 09 — Final Integration Review and Release-Readiness Report

Continue on branch:

```text
hardening/ina3221-industry-readiness
```

Start clean. If dirty, stop and report dirty files.

This is the final integration chunk for the hardening branch. Do not add large new features unless needed to fix integration regressions.

## Spawn subagents

Use:

1. `integration-review-agent`
2. `release-readiness-agent`
3. `test-fault-agent`
4. `docs-contract-agent`

## Goals

1. Review all phase changes end-to-end.
2. Fix integration issues.
3. Run full validation.
4. Create a comprehensive final hardening report.
5. State honestly whether the repo is merge-ready and what still blocks industry-grade claims.

## Full diff review

Review:

```bash
git status --short
git log --oneline --decorate -n 20
git diff main...HEAD --stat
git diff main...HEAD
```

Check for:

- accidental broad formatting,
- source changes outside scope,
- Arduino/ESP-IDF leakage into core,
- public API breakage not documented,
- missing tests for new APIs,
- docs contradicting implementation,
- validation claims not backed by commands/logs,
- stale exploration TODOs that are now fixed,
- examples claiming production status incorrectly.

## Required final checks

Run all available:

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

If hardware is connected and the CLI is available, run only safe smoke tests and record exact logs. Do not invent hardware validation.

Suggested safe hardware commands, if applicable:

```text
version
scan
probe
cfg
state
timing
read
readv
selftest
stress 100
```

Only run alert/PV/TC hardware tests if safe wiring is known.

## Final report

Create:

```text
docs/INA3221_INDUSTRY_HARDENING_FINAL_REPORT.md
```

Required structure:

```markdown
# INA3221 Industry Hardening Final Report

Date:
Branch:
Starting commit:
Final commit:

## Executive Summary

## What Changed By Phase

### Phase 01 — Baseline
### Phase 02 — Core Contracts
### Phase 03 — Alert Side Effects
### Phase 04 — Timing Model
### Phase 05 — Reset/Recover Cache
### Phase 06 — PV/TC/Summation/Safety
### Phase 07 — Fault Tests
### Phase 08 — Docs/Examples/Validation

## Public API Changes

List every added/changed/deprecated/removed API.

## Behavioral Changes

Include:
- Mask/Enable read-clear behavior,
- timing/readiness model,
- reset/recover reapply behavior,
- dirty/sync-needed behavior,
- PV/TC/summation contracts,
- raw-register write behavior.

## Migration Notes

Tell existing users what they must change.

## Tests Added

List major new test groups and final native test count.

## Validation Results

Exact commands and pass/fail results.

## Commands Not Run

Say why.

## Hardware Validation

Say exactly what was run. If nothing was run, say so.

## Remaining Risks

Include:
- pure ESP-IDF if not built,
- hardware alerts/PV/TC if not tested,
- fault paths not physically validated,
- long soak not run,
- API areas intentionally deferred.

## Merge Readiness

State one of:
- not ready,
- merge-ready for continued hardening,
- merge-ready as pre-production,
- release-ready only after hardware validation.

## Industry-Grade Verdict

Be blunt. Do not say industry-grade unless hardware/fault validation and docs justify it.
```

## Release notes draft

Also create:

```text
docs/INA3221_RELEASE_NOTES_DRAFT.md
```

Include:

- headline summary,
- breaking changes,
- new diagnostics,
- safety notes,
- validation evidence,
- known limitations.

## Final commit

```bash
git add docs README.md include src test examples tools .github platformio.ini CMakeLists.txt idf_component.yml
git commit -m "docs: finalize INA3221 industry hardening report"
git status --short
```

## Final response to user

Report:

- final branch,
- final commit,
- test count,
- validation pass/fail,
- whether merge-ready,
- what remains before release/industry-grade.
