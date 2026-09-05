# Independent code-audit verification

Reviewed on 2026-09-05, starting from clean `main` at `504a8b8` after fetching
all remotes and fast-forwarding from `a41e791`. `origin/main` was the newest
remote branch. The input was [CODE_AUDIT.md](CODE_AUDIT.md), including its
historical baseline claims, follow-up findings, claimed non-defects, and open
release tasks.

The audit contains **23 original findings and 3 follow-ups**, not the 22 + 3
stated in its original summary. Most remedies were already correct, but its
blanket closure claim was too strong. This review fixed five further issues:
raw-read CVRF handoff, missing Mask/Enable pre-consumption, two sample-snapshot
errors, and prefixed-character-literal handling in the timing guard. It also
repaired and expanded the HIL scenarios and corrected unsupported report claims.

## Verification method

- Three independent reviews divided the critical/high driver findings, the
  medium/low findings, and the examples/tooling. Their conclusions were checked
  against the combined diff and existing public contracts.
- Inspected the actual implementation and callers, not only test names or the
  audit's line citations. Compared historical implementations using `git show`
  at `9c18102` and the subsequent fix commits where needed.
- Ran the existing native suite before changes: **146/146 passed**. Additional
  host reproductions exercised clock speed/wrap, configuration views, disabled
  calibration, initialization diagnostics, summation preconditions, and snapshots.
- Checked device details against the bundled datasheet and TI's
  [SBOS576C datasheet](https://www.ti.com/lit/ds/symlink/ina3221.pdf), particularly
  sections 6.5, 7.3.2.4, and 7.6.2. The timing tables, register fields, scaling,
  reset values, and read-clear behavior agree; the Mask/Enable setters needed
  the pre-read described below.

## Every finding and the chosen remedy

Symbols refer to [the driver](../src/INA3221.cpp),
[public contracts](../include/INA3221/INA3221.h), and the existing
[basic](../test/test_basic.cpp) and [owner](../test/test_owner_operations.cpp)
test suites. Historical source line numbers in the input audit are not current
line numbers after this work.

| ID | Independent disposition and evidence | Action / simplest proper solution |
|---|---|---|
| C1 | Valid historical defect; current `readBlocking()` uses an absolute deadline. An external 18-case reproduction passed with 1/1,000/20,000 spins per millisecond, AVG_1/4/64, and ordinary/wrapping clocks. Existing tests also exercise a genuinely stalled clock. | Retained the deadline and stalled-clock guard. No arbitrary iteration budget or new clock abstraction. Added triggered blocking-read HIL scenarios. |
| C2 | Valid; `_applyConfigVerified()` stages the candidate and verifies Configuration before committing it. Direct reads succeed after averaging, conversion-time, mode, and channel changes. Mismatched readback remains an error. | Retained the shared verifier. Configuration setters still use two callbacks; see the Mask/Enable exception below. |
| C3 | Valid; lifecycle jobs and rebind can abandon stale legacy provenance, while sample jobs reject mixed ownership. `cancelConversion()` is bus-silent. | Retained these rules and added HIL cancellation checks. Corrected the explanation: lifecycle jobs reconcile Configuration and may skip an already-matching write. |
| H1 | Valid; float readers and the fixed-unit owner path honor the same current direction. `Config::direction` round-trips through the profile. | Retained the small float sign helper; converting through whole milliamps would unnecessarily lose precision. |
| H2 | Valid after the audit's polarity correction: the original error was retaining TCF with sticky OR. `_retainMaskEnable()` now stores the latest level and derives its inverse. | Kept the hardware-latched condition without adding a software latch. Corrected the unconditional initialization claim. Added HIL checks accepting either complementary level/fault pair. |
| H3 | Valid; `_lastCallbackInvoked` prevents a driver-side deadline rejection being called an ambiguous write. Adapter-local `INVALID_PARAM`/`INVALID_CONFIG` still explicitly mean no backend access. | Retained the documented adapter obligation. Invocation alone cannot reveal an adapter's internal transfer phase; no extra transport interface is needed. |
| H4 | Valid; `readShuntSumRaw()` checks shunt mode, selected channels, and verified configuration before I2C. Empty selection, bus-only mode, and unknown alert configuration reject correctly. | Retained the gates. Fixed HIL steps that still expected successful reads with no selected summation channel; added bus-silent rejection and selection/restore checks. |
| H5 | Valid; the Arduino adapter rejects unsupported callback bounds as `INVALID_CONFIG`, and all three owner poll sites use `wireSafeTransferBudget()`. | Retained the fixed-timeout cap. Added a requested-budget-255 HIL step. IDF keeps its real per-call timeout behavior. |
| M1 | Valid; `_legacyConfigView` represents observed Configuration while `_profile` remains desired state. `writeConfig(0x4FFB)` retains that distinction and performs no premature readiness read at 100 ms. | Kept the observed view and reference-returning API. Deleting it would discard useful raw-diagnostic semantics without simplifying the actual contract. |
| M2 | Valid; a verified individual alert setter preserves prior family certainty, since one register cannot verify all ten alert registers. | Retained the shared verifier and replaced its identity ternary with `_alertConfigState = priorState`. Added the required Mask/Enable pre-read. |
| M3 | Valid; `_finishJobFailure()` consistently maps interior deadlines to `TIMED_OUT`, while reporting hardware effects separately. Deadline and cancellation matrices pass. | Retained the centralized classification. No duplicated per-stage outcome logic. |
| M4 | Valid; tracked initialization transfers now update counters/errors/timestamps while state stays `UNINIT` until success. A failing first `begin()` independently reproduced one retained `I2C_BUS` failure and its timestamp. | Retained the state-only initialization guard. |
| M5 | Valid; `_handleResetWriteEffect()` preserves real untaken evidence and distinguishes confirmed reset defaults from ambiguous effects. Reset tests pass. | Kept the narrower remedy. Clearing the whole alert record would silently acknowledge events; condition levels remain documented as stale until observed again. |
| M6 | Valid; permanent-CVRF-low behavior uses a fixed 50 ms recheck and waits without I2C when another bounded read cannot fit. | Kept the simpler fixed interval. Fixed the associated conversion-start snapshot being moved by a recheck. |
| M7 | Valid; the native example's fixed-name component shim resolves the repository root, while CI mounts `/component-source`. Contract checks verify the shim and path. | Kept the existing shim. No additional component abstraction. Full native-IDF build coverage is distinguished from local static checks below. |
| L1 | Valid; `encodeSignedField()` clamps finite values before `lrintf()`. Independent extreme-value checks passed. | Retained float-domain saturation. NaN and infinities deliberately map to zero under the existing nonfinite-input contract. |
| L2 | Correct cleanup; masked power-valid values cannot reach the removed negative guard. | No further change. |
| L3 | Valid; enabled channels require representable positive calibration, while invalid disabled-channel values normalize to zero. Zero, negative, NaN, infinities, submicroohm and oversized values were checked. | Kept validation in the existing legacy-to-profile conversion. |
| L4 | Correct documentation remedy. Reserved and transport-supplied errors retain their numeric positions; `MEASUREMENT_NOT_READY` aliases a value the driver does produce. | No enum removal or renumbering. |
| L5 | Valid; historical members `_jobReadBusNext`, `_jobStatus`, and `_jobHardwareEffect` had writes without readers and are absent now. | Kept the deletion. |
| L6 | Valid structural objection; Mask/Enable setters compose desired bits from `_profile`, not the observation cache. Raw writes still make uncertainty observable. | Kept profile composition and added pre-consumption before changing settings. No second desired-state cache. |
| L7 | Intended behavior, not a driver defect. Successful power-down becomes the retained desired mode; reconcile/recover retain it. | Kept the documented contract and explicit apply-profile wake-up path. |
| L8 | Most listed drift was fixed, but `PollJobSnapshot::conversionReady` and `conversionStartMs` still had edge-case errors. See the seven-item breakdown below. | Fixed the two snapshot errors in their existing owners and added regressions. |
| F1 | Incomplete at the review baseline. Typed setters and `readAlertFlags()` handed off CVRF, but raw `readRegister16(REG_MASK_ENABLE)` still consumed it without updating legacy readiness. | Moved handoff into the common successful `_readMaskEnableWithTimeout()` path and removed redundant caller updates. The new raw-read regression failed before this fix. |
| F2 | The digit-separator fix worked, but valid prefixed character literals could still hide subsequent timing calls. | Extended the existing literal recognizer for `L`, `u`, `U`, and `u8`, with adversarial self-tests. No parser dependency. |
| F3 | Correctly fixed at all three Arduino poll sites; the static helper-per-`PollContext` check remains active. | No firmware change needed. Added HIL coverage of the largest requested budget. |

L8's seven original items were checked individually:

1. `readBlocking()` capture uptime uses the caller's extended absolute domain,
   including 32-bit wrap; retained.
2. The README compatibility table records verified writes and destructive
   readiness reads; updated again for the three-callback Mask/Enable setters.
3. `NowMsFn` documents zero health timestamps without a clock hook; retained.
4. The comment/string stripping-order fix was present; the separate prefixed
   literal bypass is now fixed under F2.
5. `bind()`'s successful postconditions and pending-result admission guard are
   consistent; retained.
6. Snapshot channel cursors were live, but readiness and the initial conversion
   start needed the additional corrections described below.
7. Ignored build output exists under `.pio/`; that is not source. The fix-commit
   history does not substantiate a special ignored-leftover cleanup. No broad
   cache deletion was performed or required for source correctness.

## Additional fixes and why these implementations are sufficient

### One owner for consuming-read readiness

The failing sequence was `startConversion()` followed by a raw Mask/Enable read
that observed CVRF, followed by `readConversionReady()`. The hardware flag was
already cleared, so readiness could remain false forever. Every successful
consuming read now retains alerts and hands off CVRF in the same helper.
Readiness and direct measurement then succeed without another Mask/Enable read.
Owner-job admission already clears incompatible legacy provenance, so this
does not permit mixed ownership.

### Observe flags before changing Mask/Enable settings

The typed setters previously wrote and then verified Mask/Enable. Section
7.6.2.16 requires consuming the prior flags before changing warning settings.
The shared verifier now performs **read, write, readback** for this register;
the candidate profile commits only after matching verification. Failure of the
first read prevents the write. Both reads retain real observations, including
CVRF. Other setters retain their two-callback bound.

Regression tests assert the three callbacks for both setters, no write after a
failed pre-read, and retained readiness/events when the subsequent write gets an
address NACK. Generation and desired state remain unchanged on failure.

### Preserve observed snapshot facts

Continuous snapshots previously reported CVRF=false after a sample observed
CVRF=true. Triggered snapshots only copied readiness on complete success, losing
the diagnostic observation when a later data read failed. Both compatibility
poll paths now publish readiness from the current sample's observed alert record.

CVRF-low rechecks reused the wait-origin stage and overwrote the initial
conversion start. That field is now assigned only before the first alert
observation; the existing alert-valid state distinguishes the initial wait from
rechecks without adding another member. Regressions cover both sampling modes,
success and channel-read failure, and stable start time through a recheck.

### Keep the timing guard and HIL suite honest

`L'('` followed by `millis()` and another character literal caused the guard to
mistake the first closing quote for an opener and hide the call. Prefix-aware
literal recognition closes that case while preserving digit separators.

The HIL suite now has **404 default steps**, or **407 with benchmarks**. It
adds triggered AVG_1/AVG_4 blocking reads, legacy cancellation and its zero-I2C
assertion, two-/three-callback setter assertions, measurement without intervening
recovery, a budget-255 poll, valid/invalid summation cases, and complementary
timing-control level/fault checks. Either TC state is accepted: rail sequencing
can legitimately latch the condition. Parser tests reject inconsistent pairs
and missing verification transfers. No new firmware command was required.

## Other report claims and release tasks

- The original audit's H2, M1, M5, H1, and H3 changes of remedy are reasonable:
  preserving precision, evidence, observed state, and explicit transport
  obligations is simpler than adding redundant latches or replacing public APIs.
- Initialization does not always disable timing control. A matching
  Configuration can skip its write, and the disabling condition depends on when
  an actual write occurs. The README already described this condition correctly;
  the audit has been corrected to match.
- The successful-path transfer ceilings remain 35/33/33/8/7/3 for the six job
  kinds with three channels. The new pre-read affects synchronous Mask/Enable
  setters, not cooperative jobs, which already read before conditional writes.
- Profile application is not atomic. Reconciling Configuration before alert
  limits can expose intermediate settings; POR defaults do not prove otherwise
  for a previously configured device. The audit's categorical safety argument
  was narrowed. Observable partial effects and owner policy remain the contract.
- **Release version remains a release task.** The next release needs `3.2.0`
  for the additive public APIs, with the corrected TCF snapshot behavior called
  out for integrators. This review leaves changes under `Unreleased` and keeps
  metadata/install examples at the existing published `v3.1.0`; it does not
  create a release tag or point installation instructions at a nonexistent tag.
- **Hardware qualification remains pending.** The old 379-step evidence ledger
  predates the fixes. The suite is repaired and host-checked, but no new physical
  result is claimed. See [HIL.md](HIL.md) for the expanded workflow and limits.

## Validation performed

| Check | Result |
|---|---|
| Baseline native tests | 146/146 passed |
| Native tests after fixes | 151/151 passed; five added test functions include multiple scenarios |
| Additional host reproductions | Clock matrix, raw-view timing, initialization health, shunt validation, summation gates, and snapshot before/after cases passed |
| Strict host warnings | `check_strict_compile.py` passed with GCC, including conversion/sign-conversion warnings as errors |
| Static contracts | Core timing, Arduino CLI, and native-IDF example checks passed |
| HIL host checks | Parser self-test and 404-step dry-run passed; step IDs unique, final health retained |
| Arduino ESP32-S3 and ESP32-S2 | Both PlatformIO environments built successfully |
| Documentation / metadata | Doxygen warning-clean; generated-version and release-metadata consistency checks passed |
| Packaging | `scripts/pio.cmd pkg pack` passed; tarball written outside the repository |
| Native ESP-IDF compilation | No local IDF 6.x, Docker, or installed WSL; the existing CI job builds both targets from `/component-source` |
| Physical HIL / electrical faults | Not run; no fixture was identified for this review |

Windows build commands used the required `scripts/pio.cmd` and the existing
PlatformIO Core 6.1.19. The inherited `PLATFORMIO_CORE_DIR=C:\pio` referred to a
stale Python 3.11 environment and failed dependency setup under Python 3.12.
For the build process only, selecting the existing user-managed directory and
its installed dependencies resolved the failure; no PlatformIO Core was installed:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $env:USERPROFILE '.platformio'
$env:PLATFORMIO_OFFLINE = '1'
.\scripts\pio.cmd test -e native
.\scripts\pio.cmd run -e esp32s3dev -e esp32s2dev
```

The full check set also includes `git diff --check`, all three contract scripts,
`check_strict_compile.py`, `scripts/generate_version.py check`,
`check_metadata_consistency.py`, `hil_cli_runner.py --parser-self-test`,
`hil_cli_runner.py --dry-run`, `doxygen Doxyfile`, and `scripts/pio.cmd pkg pack`.
Generated build, documentation, and package artifacts are ignored or external.
