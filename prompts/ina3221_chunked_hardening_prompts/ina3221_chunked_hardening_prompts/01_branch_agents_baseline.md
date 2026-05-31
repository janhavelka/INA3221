# Prompt 01 — Branch, Baseline, AGENTS.md, and Work Plan

You are working in the INA3221 repository after the deep industry-readiness exploration report.

This is the first implementation chunk toward an industry-standard INA3221 I2C library. Do **not** implement fixes yet. Prepare the branch, rules, baseline evidence, and phase plan.

## Starting point

The exploration report says the core architecture is good, but production blockers remain:

- hidden `REG_MASK_ENABLE` read-clear side effects,
- questionable conversion timing/readiness model,
- reset/recover not restoring alert/PV thresholds,
- incomplete PV/TC/summation contracts,
- weak fault-injection tests,
- missing thread/ISR and raw-register contracts,
- missing hardware and pure `idf.py` validation.

## Branch

Start from a clean working tree.

```bash
git status --short
git branch --show-current
```

If dirty, stop and report the dirty files.

Then:

```bash
git checkout main
git pull --ff-only
git checkout -b hardening/ina3221-industry-readiness
```

If `hardening/ina3221-industry-readiness` already exists, do not delete it. Switch to it only if it is clearly the intended active branch.

## Update AGENTS.md

Edit or create `AGENTS.md` in the repo root.

Add rules:

- Core code in `include/` and `src/` must remain framework-neutral.
- No Arduino, Wire, ESP-IDF, FreeRTOS, Serial, String, framework logging, task ownership, pin ownership, or global bus ownership in core.
- Core must use injected/non-owning I2C callbacks.
- Public fallible APIs should return `Status` or have a documented lossy/convenience contract.
- Driver instance is not internally thread-safe unless explicitly changed and tested.
- Public I2C/blocking APIs are not ISR-safe.
- Transport callbacks must not recursively call back into the same driver instance.
- Any operation that reads `REG_MASK_ENABLE` must treat it as destructive/read-clear for CVRF and alert flags.
- Any raw register write that can desync cached state must be documented and must mark dirty/sync-needed state if appropriate.
- No hardware validation claim is allowed unless real hardware logs are captured.
- No pure ESP-IDF validation claim is allowed unless `idf.py` or CI actually builds the IDF example/component.

Add planned subagents:

1. `core-contracts-agent`
2. `alert-side-effects-agent`
3. `timing-model-agent`
4. `reset-recover-cache-agent`
5. `pv-tc-summation-agent`
6. `test-fault-agent`
7. `docs-examples-idf-agent`
8. `integration-review-agent`

## Baseline inventory

Inspect and summarize:

- `include/INA3221/*.h`
- `src/INA3221.cpp`
- `test/test_basic.cpp`
- `examples/01_basic_bringup_cli/main.cpp`
- `examples/esp_idf/basic/**`
- `tools/*.py`
- `README.md`
- `docs/**`
- `.github/workflows/ci.yml`
- `platformio.ini`
- `CMakeLists.txt`
- `idf_component.yml`

## Run baseline checks

Run all available checks. Do not invent results.

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

If ESP-IDF is available:

```bash
idf.py --version
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

If `idf.py` is missing, record that exactly.

## Output for this chunk

Create:

```text
docs/INA3221_HARDENING_PHASE01_BASELINE.md
```

Include branch name, starting commit, AGENTS.md changes, subagents planned, baseline architecture summary, exact command results, commands not run and why, and pre-existing issues found before implementation.

## Commit

Commit only baseline/report/AGENTS.md changes.

```bash
git add AGENTS.md docs/INA3221_HARDENING_PHASE01_BASELINE.md
git commit -m "docs: establish INA3221 industry hardening baseline"
git status --short
```

Stop after this chunk and report back.
