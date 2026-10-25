# INA3221 software follow-up prompts

Prepared 2026-10-25 after reviewing source at
`40838b56e41c25183478b086e9e214ceffe3290c` (`main`) and the bundled SBOS576C
datasheet. This is an implementation task queue, not a statement that fixes
have been applied. Read [AGENTS.md](../../AGENTS.md) and the
[retained-branch review](../RETAINED_BRANCH_AUDIT.md).

| Order | Prompt | Finding coverage | Intended change | Status |
| --- | --- | --- | --- | --- |
| 01 | [Power-valid integration contract](01-power-valid-contract.md) | PV-1, original H3/L1; preserve TIM-1 boundary | Complete user-facing/public API guidance, with software changes only if the new audit proves a concrete defect | NOT IMPLEMENTED |

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

After execution, update PV-1 and this row with the resolution commit and actual
verification, keeping TIM-1/HIL-1 open unless independently resolved with
adequate evidence. Prompt creation itself closes none of those findings.
