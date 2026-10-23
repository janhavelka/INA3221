# Retained branch review

Reviewed on 2026-10-23 against `main` at
`e45622dbaa5bb0b3151d980f30a1b6ea1cdb1a1b`. Origin was fetched, main matched
`origin/main`, and the initial worktree was clean. This was the newest active
branch. All comparisons below describe that baseline, before this document's
commit. No retained branch was merged, changed, or deleted.

## Decision

The unmerged audit branch contains two documentation commits and **no driver,
test, example, or workflow implementation**. The native IDF port and later
owner-engine hardening are already ancestors of main. Most original findings
have been addressed by the current cooperative engine and subsequent audit
fixes; an old unmerged report does not mean those fixes are absent.

One integration-contract gap remains worth carrying forward: validated
power-valid thresholds do not establish a valid board-level power-good signal.
The bus-measurement, unused-channel wiring and PV-output electrical conditions
are not explained in the current public integration guidance. The original
high-averaging timing question also remains an evidence question, not a proven
current timing bug. Details and acceptance criteria follow.

**Recommendation:** archive and retire the exploration branch once the open
items in this document have an active owner. The branch contains useful research,
not a patch to merge. Already-contained port/hardening branch names can retire
separately. No deletion was performed.

## Exact inventory and unique commits

Counts are main-only / retained-tip-only commits from
`git rev-list --left-right --count <main>...<tip>`.

| Retained `origin/` branch | Full tip SHA | Counts | Disposition |
| --- | --- | --- | --- |
| `audit/ina3221-industry-readiness-exploration` | `2b3093b7de04e6a0c4cff4c604e125c1dcd9dc62` | 38 / 2 | Unmerged research/prompts; semantic findings below. |
| `idf-port` | `7e14e63671098b6bda525b09814feda6c2efbf38` | 41 / 0 | Ancestor of main; port already present. |
| `hardening/tunnelmonitor-suitability-reaudit` | `a38b1b8baded9bdea387ec78e6eaff80324157c6` | 22 / 0 | Ancestor of main; local branch matches remote tip. |

All remote branches were inspected; `origin/HEAD` only aliases main.

| Unique commit | Actual patch contents | Conclusion |
| --- | --- | --- |
| `70e563301dd85845c3a20a6d91d10f8decb43b59` | Despite its generic `Implement feature X...` subject, adds only the 1,039-line `docs/INA3221_INDUSTRY_READINESS_EXPLORATION.md`. | No implementation to recover. |
| `2b3093b7de04e6a0c4cff4c604e125c1dcd9dc62` | Adds sequenced hardening prompts and a manifest under `docs/prompts/`. | Proposed work, not proof that it remains undone. |

Both appear as `+` in `git cherry`; that is patch uniqueness, not behavioral
absence. The combined three-dot diff changes twelve documentation files only.

## Reconciliation of every original finding

IDs refer to the exploration report at the unique SHA above, **not** the later
findings with similar IDs in [CODE_AUDIT.md](CODE_AUDIT.md). Evidence line numbers
are for the reviewed main snapshot; named functions/tests are stable anchors.

| Original finding | Current disposition | Evidence on main |
| --- | --- | --- |
| H1: readiness silently consumes alert evidence | Implemented. Every consuming read retains CF/SF/WF/CVRF evidence; peek/take and uncertainty are explicit, including raw reads and verifier reads. | `src/INA3221.cpp:871` alert retention; `README.md:256`; tests `test_typed_mask_enable_setters_preserve_observed_conversion_ready`, `test_raw_mask_enable_read_preserves_observed_conversion_ready`, `test_owner_alert_events_are_retained_and_taken_exactly_by_owner`. Later closeout F1 covers the once-missed paths. |
| H2: initialization always disables TC | Original blanket claim is corrected. The owner engine conditionally writes configuration; TC depends on whether/when a write occurs. Current README explicitly warns about sequence timing and exposes latest raw TCF plus inverted fault. No automatic TC-preserve workflow is promised. | `README.md:265`; `docs/CODE_AUDIT.md` H2 correction; `src/INA3221.cpp:877`. TC-dependent products still need a controlled reset/startup sequence and physical test. |
| H3: raw/incomplete power-valid support | Partially implemented: ordered bounded millivolt windows and full profile verification exist. Board/mode validity remains an integration-documentation gap, PV-1 below. | `src/INA3221.cpp:398`, `:493`, `:2732`; `include/INA3221/INA3221.h:1012`; `README.md:100`. |
| H4: reset/recovery loses thresholds | Implemented with a stronger explicit contract. Complete desired profile retains all managed alert/PV limits; reset marks certainty dirty and reconcile/recover reapplies it. Reset itself is not silently treated as a completed replay. | `src/INA3221.cpp:1003` (`_desiredRegister`), `:2617` reset; tests `test_owner_initialize_full_profile_exact_sequence_and_verification`, `test_owner_apply_profile_failure_at_every_transfer_stage`, `test_raw_reset_updates_both_profile_certainty_families`. |
| H5: averaging multiplier may delay CVRF unnecessarily | Current design deliberately uses a conservative maximum cycle multiplied by averaging and validates deadline admission. Do not implement the old proposed scan-only gate without evidence. The relationship between earliest CVRF and the conservative gate is still worth measuring: TIM-1 below. | `src/INA3221.cpp:252` (`maximumCycleTimeUs`); `README.md:360`; `test_owner_wait_uses_maximum_timing_and_crosses_uint32_boundary_without_io`; current hardware ledger does not qualify the expanded suite. |
| M1: copying/moving driver | Implemented: all four operations deleted. | `include/INA3221/INA3221.h:293`. |
| M2: bool/sentinel convenience APIs | Superseded for production by Status-returning jobs/results and explicit profiles. Legacy `conversionReady()`, `getChannelEnable()` and `getShuntResistance()` remain compatibility conveniences; callers needing unambiguous failure must use explicit status/profile contracts. Their existence is not a lost patch. | `include/INA3221/INA3221.h:614`, `:748`, `:760`; README compatibility section and owner API. |
| M3: probe collapses transport failures | Implemented. Raw transport failures return unchanged; wrong manufacturer/die use distinct mismatch statuses. | `src/INA3221.cpp:1823`; owner identity stages at `:1316`, `:1334`. |
| M4: writes to read-only registers accepted | Implemented: separate writable-register predicate rejects them before I/O. | `src/INA3221.cpp:73`, `:3089`. |
| M5: unequal-shunt current summation | Scope narrowed: exported sum APIs return raw shunt voltage or millivolts, not aggregate current. They check selected/enabled channels and certainty. A sum-current helper is optional; unequal resistances do not invalidate voltage summation. Add an explicit warning if documenting aggregate-current use. | `include/INA3221/INA3221.h:569`; `src/INA3221.cpp:2032`. |
| M6: plausible default calibration | Implemented for production. Default `DeviceProfile` shunts are zero and enabled channels require explicit calibration. Legacy Config defaults remain compatibility behavior. | `include/INA3221/Config.h:139`; `src/INA3221.cpp:513`; `test_owner_bind_requires_explicit_calibration_only_for_enabled_channels`. |
| M7: thread/ISR contract | Implemented: serialized single owner, no copy/move/reentrancy/ISR access. | `README.md:395`; public class contract. |
| M8: IDF no lock/imprecise NACK | Resolved as an ownership/backend contract. Application owns serialization; driver/adapter must not add hidden locks/retries. Combined-transfer NACK phase remains unknown and native detail is preserved. Both native-IDF CI targets exist. | `docs/IDF_PORT.md:33`; `.github/workflows/ci.yml` `esp-idf-build`. |
| M9: Wire ignores callback timeout | Implemented: fixed-timeout adapter rejects tighter requests before backend access; owner poll sites cap admission. | `examples/common/I2cTransport.h:60`; later closeout H5/F3 and CLI checker. |
| M10: simplistic fake | Implemented: scripted address/shape/timeout assertions, queued failures, read-clear behavior and per-stage fault/cancellation matrices. | `test/support/ScriptedTransport.h`; `test/test_owner_operations.cpp:448`, `:765`, `:1989`. |
| L1: board safety guidance | Partly addressed by Kelvin/shunt calibration guidance; board-specific input/pull-up/PV conditions still require the datasheet. Include explicit input-rating and PV wiring guidance when closing PV-1. | `README.md:57`; bundled datasheet; PV-1 below. |
| L2: raw value ambiguity | Public raw methods identify datasheet-format register values; fixed-unit production results avoid count/word ambiguity. | Public header raw-read/encode contracts; README sample units. |
| L3: narrow static guard | Expanded and later hardened against lexical bypasses. Remains lexical, not a preprocessor or whole-program analysis. | `tools/check_core_timing_guard.py`; later closeout F2. |

The prompt set also proposed optional SMBus alert-response support, a
TC-preserving lifecycle, and a current-summation helper. No implementation of
those features is stranded on the branch, and the current API does not promise
them. Add only when an actual consumer requires them.

## Open actionable items

### PV-1: explain when the PV signal is meaningful

**P1 for a product using PV as a power-good decision; otherwise P2 integration
documentation.** Confirmed documentation/contract gap, not a proven fault in the
register encoder. `_validateProfile()` accepts shunt-only/partially enabled
profiles while validating an ordered PV window; `validatePowerValidWindow()`
only validates numeric limits. `AlertFlags::powerValid` is the raw PVF bit, not
a certification of fresh board-level rail state. The current public README and
PV setter documentation omit the needed qualification conditions.

The bundled [TI SBOS576C datasheet](INA3221_datasheet.pdf), section 7.3.2.4,
printed pages 13-14, independently confirms: PV evaluates all three bus rails,
unused-channel IN- must connect to a used rail and unused IN+ must float, and
periodic bus-voltage conversions are needed to keep the result current. VPU/PV
interface voltage also needs explicit board-level treatment. These constraints
were re-read from the bundled PDF during this audit.

Acceptance criteria:

- Document these conditions beside public PV use, distinguish raw PVF/threshold
  access from qualified power-good policy, and cite the datasheet sections.
- Explain that numeric profile validation does not validate wiring or freshness;
  do not reject all shunt-only profiles merely because they retain PV limits.
- For a consumer requiring qualified PV, validate bus-measurement mode, selected
  rail topology, sample cadence and output electrical interface in that consumer
  or a narrowly scoped optional helper. Test unsupported modes/topologies and
  verify threshold/hysteresis/PV pin behavior on the actual board.

### TIM-1: resolve earliest-CVRF versus conservative averaging wait

**P2 evidence/performance question, not a confirmed implementation defect.**
The current worst-case three-channel, shunt+bus, AVG_1024 gate is 55,713,792 us
before the wake margin. The old report suggested removing the averaging factor.
The bundled datasheet describes an iterative averaging filter (7.4.1), a scan
period (7.4.2.1), samples in AVG bits (7.6.2.1), and CVRF after all conversions
(7.6.2.10); these statements alone do not establish the earliest CVRF observed
for every triggered configuration. Current tests establish the chosen software
contract, not physical earliest-ready timing.

Acceptance: capture trigger-to-CVRF timing at AVG_1/4/16/1024, representative
conversion times, channel counts and signal modes on identified hardware; record
source/build, repeated samples and analyzer evidence. Compare with both scan
time and configured maximum gate. Only change admission/poll timing after that
evidence or an authoritative clarification, retaining deadline and destructive
alert-read tests. No timing change is proposed by this audit.

### HIL-1: qualify the current implementation

**P1 before a current hardware-qualification claim.** [HIL.md](HIL.md) explicitly
marks 3.2.0 qualification NOT RUN; the retained 379-step/3.1.0 result predates
audit fixes and the expanded 404-step suite. Execute that existing suite on an
identified fixture, then separately qualify physical pins, analog accuracy,
faults, all claimed targets/frameworks, and the PV/TC scenarios above. Keep exact
commit/firmware identity, artifact hashes and unrun cases in the existing ledger.

## Reproduction and workflow

```powershell
git status --short --branch
git for-each-ref --format="%(refname:short) %(objectname)" refs/heads refs/remotes/origin
git rev-list --left-right --count e45622dbaa5bb0b3151d980f30a1b6ea1cdb1a1b...origin/audit/ina3221-industry-readiness-exploration
git cherry e45622dbaa5bb0b3151d980f30a1b6ea1cdb1a1b origin/audit/ina3221-industry-readiness-exploration
git log --stat e45622dbaa5bb0b3151d980f30a1b6ea1cdb1a1b..origin/audit/ina3221-industry-readiness-exploration
git show 70e563301dd85845c3a20a6d91d10f8decb43b59:docs/INA3221_INDUSTRY_READINESS_EXPLORATION.md
git merge-base --is-ancestor origin/idf-port e45622dbaa5bb0b3151d980f30a1b6ea1cdb1a1b
```

1. Before retirement, fetch again and verify the recorded tips have not moved.
2. Assign PV-1/TIM-1 and retain HIL-1 in the existing hardware ledger; do not
   cherry-pick old prose as if it contained implementation.
3. Archive the non-ancestor tip using a named durable remote reference and
   verify retrieval before removing the active branch name in a separate
   explicit maintenance action.
4. Make any selected fix against current main. Preserve the later audit fixes
   and current owner/compatibility contracts.

## Verification and future CI

Actually completed: branch/count/cherry/diff inspection; semantic source/test
review; original finding and current closeout comparison; local datasheet PDF
review; workflow inspection. The following three checks passed in this review:

```powershell
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
```

Final `git diff --check`, local Markdown links, and every full commit SHA were
checked and passed. Native tests, builds, HIL and remote CI were not rerun for this
documentation-only review; historical results above are explicitly historical.

For a later substantial implementation, use `.\scripts\pio.cmd test -e native`
on Windows plus `tools/check_strict_compile.py`, the three contracts, metadata
and version checks, HIL parser, Doxygen, package validation, both Arduino targets
and both native-IDF targets from `.github/workflows/ci.yml`. CI uses Ubuntu
24.04, PlatformIO 6.1.19 and native IDF 6.0.1. Verify the exact final commit's
terminal CI results when that change warrants remote CI; never substitute this
documentation audit or an older green run.
