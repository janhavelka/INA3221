# INA3221 software follow-up prompts

Prepared 2026-10-25 after reviewing source at
`40838b56e41c25183478b086e9e214ceffe3290c` (`main`) and the bundled SBOS576C
datasheet. This task queue records execution status below; preparation alone
did not apply a fix. Read [AGENTS.md](../../AGENTS.md) and the
[retained-branch review](../RETAINED_BRANCH_AUDIT.md).

| Order | Prompt | Finding coverage | Intended change | Status |
| --- | --- | --- | --- | --- |
| 01 | [Power-valid integration contract](01-power-valid-contract.md) | PV-1, original H3/L1; preserve TIM-1 boundary | Completed README/public API guidance; re-audit found no runtime correction needed | IMPLEMENTED 2026-10-29 — [`ee4ca7d4d1df81bbe0c3c58c1a19ac8c0d2ee12a`](https://github.com/janhavelka/INA3221/commit/ee4ca7d4d1df81bbe0c3c58c1a19ac8c0d2ee12a) |

This repository does not need a new generic power-good service, bus owner,
sampling scheduler or profile restriction to close the documented PV gap.
Prefer accurate, consolidated contracts beside the existing APIs. Do not turn
a documentation correction into a speculative driver refactor.

Execute 01 on the existing development branch. Re-audit current code, follow
the evidence, complete the relevant edits/checks, then commit and synchronize.
Preserve unrelated changes. Keep current transport ownership, bounded jobs,
read-clear alert retention and fixed-unit encoding behavior.

**Scope boundary:** TIM-1 asks when physical CVRF first asserts relative to
averaging. It is not a proven software defect and cannot be closed with a fake
whose behavior is chosen to match a desired answer. The prompt keeps that
boundary visible, but does not order a speculative timing reduction or a
hardware campaign. HIL-1 is also a separate physical qualification queue.

PV-1 verification passed: bundled PDF text/figures and source/test/consumer
review; local links and anchors; `check_core_timing_guard.py`,
`check_cli_contract.py`, `check_idf_example_contract.py`,
`scripts/generate_version.py check`, `check_metadata_consistency.py`,
`doxygen Doxyfile` (no warnings), and `git diff --check`. The corrected primary
citation is bundled SBOS576C (September 2025), section 7.3.2.3, pages 13–14;
PVF uses section 7.6.2.16, Table 7-36, page 34.

See the [PV-1 closeout](../RETAINED_BRANCH_AUDIT.md#pv-1-explain-when-the-pv-signal-is-meaningful)
for scope and consumer-specific qualification. Runtime behavior is unchanged;
native tests, strict compile and firmware builds were not rerun, and remote CI
was not separately dispatched or awaited. No hardware run is claimed. **TIM-1 and
HIL-1 remain OPEN**, including physical proof for any consumer relying on PV.
