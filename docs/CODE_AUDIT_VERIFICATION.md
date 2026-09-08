# Independent code-audit verification

Reviewed on **2026-09-08**, starting from clean `main` at `6b64b9a`.
All remotes were fetched and `main` was fast-forward checked against
`origin/main`; it was already current and was the newest remote branch.

All **23 original findings and 3 follow-ups** in [CODE_AUDIT.md](CODE_AUDIT.md)
were checked against the implementation and tests. Most fixes were already
correct. This pass found and fixed two further driver issues and three timing
guard gaps; it also corrected stale audit claims. The
[2026-09-05 verification](https://github.com/janhavelka/INA3221/blob/6b64b9a/docs/CODE_AUDIT_VERIFICATION.md)
remains available in Git history.

## Review method

Three reviewers independently examined critical/high behavior, medium/low
driver contracts, and examples/tooling. Their findings were checked against the
combined diff. Review included callers, failure paths, existing regressions,
historical code where relevant, and independent host reproductions.

The baseline native suite passed **151/151** tests. Additional host checks
covered 18 clock/profile/wrap combinations, 26 typed-setter/read combinations,
all channels and both current directions including fractional current,
consuming-read readiness handoff, reset evidence, configuration views, and
deadline/ownership failures. The actual Arduino budget helper was compiled and
checked over 3,328,256 requested-budget/deadline combinations plus boundary cases.

Register fields, scaling, reset values, timing tables, summation update rules,
and read-clear/TCF behavior were checked against the bundled datasheet and TI's
[SBOS576C datasheet](https://www.ti.com/lit/ds/symlink/ina3221.pdf), sections 6.5,
7.3.2.4 and 7.6.2. No register-map or conversion-table change was needed.

## Every finding and the simplest proper remedy

Symbols below refer to [the implementation](../src/INA3221.cpp),
[public contracts](../include/INA3221/INA3221.h), and the existing
[basic](../test/test_basic.cpp) and
[owner](../test/test_owner_operations.cpp) test suites. Historical line
citations in the input audit refer to earlier revisions.

| ID | Current verification | Decision / action |
|---|---|---|
| C1 | Valid historical bug; `readBlocking()` now uses extended absolute time. Independent AVG_1/4/64, 1/1,000/20,000 spins per millisecond, and wrap cases all passed. | Keep the deadline and stalled-clock guard. An iteration-only budget would reintroduce the bug. |
| C2 | Valid and fixed. `_writeManagedRegisterVerified()` verifies Configuration before committing the candidate; direct reads remain available after successful setters. | Keep the shared two-callback verifier. Readback failure preserves uncertainty. |
| C3 | Valid and fixed. Lifecycle jobs/rebind discard legacy conversion bookkeeping; sample jobs reject mixed ownership; `cancelConversion()` is bus-silent. | Keep existing admission and cancellation rules; no recovery state machine needed. |
| H1 | Valid and fixed. Float readers use `applyCurrentDirection()`; profile and legacy directions agree on all channels, including fractional current. | Keep the sign operation. Routing floats through whole-milliamp results loses precision. |
| H2 | Original polarity title was incorrect; sticky-OR retention was the defect. Latest TCF level and inverse fault are now retained correctly. | Keep the hardware latch without adding a redundant software latch. |
| H3 | Valid and fixed within the documented adapter contract. A pending write rejected at a driver deadline after a previous successful read gives `TIMED_OUT/NONE` without a callback. | Keep invocation tracking and explicit bus-silent validation statuses. Invocation alone cannot reveal an adapter's internal transfer phase. |
| H4 | Valid and fixed for invalid configurations. Empty selection, bus-only/power-down modes, and unknown alert certainty reject before I2C. | Keep existing gates. Clarify that changing summation selection requires a new completed cycle; direct register reads do not create a fresh sample. |
| H5 | Valid and fixed. All three Arduino poll sites honor the fixed Wire timeout; unsupported adapter bounds reject as `INVALID_CONFIG`. Exhaustive helper checks passed. | Keep the shared budget cap. IDF retains true per-call timeouts; no bus reconfiguration inside callbacks. |
| M1 | Valid historical divergence; `_legacyConfigView` now intentionally represents observed settings while `_profile` is desired state. Raw-write timing uses the observed view. | Keep the observed view and reference API; deleting it would lose useful diagnostic semantics. |
| M2 | Original typed-setter bug is fixed, but read-before-write paths could still retain false `APPLIED` certainty after observing drift. | Preserve prior family certainty only while evidence supports it. Fix profile, power-down, and typed Mask/Enable pre-read paths as described below. |
| M3 | Valid and fixed. `_finishJobFailure()` centralizes deadline outcome and independent hardware-effect classification. | Keep centralized terminalization. New drift evidence remains observable when cancellation or failure prevents repair. |
| M4 | Valid and fixed. Tracked initialization transfers update counters/errors/timestamps while state remains `UNINIT` until success. | Keep the state-only initialization guard. |
| M5 | Valid and fixed. Confirmed reset updates known defaults without fabricating a consuming-read snapshot; untaken events survive confirmed/ambiguous reset. | Keep real evidence until explicitly taken. Clearing the whole snapshot would silently acknowledge events. |
| M6 | Valid and fixed. CVRF-low rechecks use 50 ms intervals and wait bus-silently if another bounded read cannot fit. Initial conversion-start time survives rechecks. | Keep the fixed interval; proportional backoff adds complexity without a demonstrated need. |
| M7 | Valid and fixed. Fixed-name IDF component shim resolves root sources independently of checkout name; CI mounts `/component-source`. | Keep the small existing shim. Static checks passed; native IDF build evidence is recorded below. |
| L1 | Valid and fixed. `encodeSignedField()` clamps finite values before `lrintf()`; nonfinite values use the existing zero contract. | Keep float-domain saturation. |
| L2 | Correct cleanup. Masked power-valid values cannot reach the removed negative guard. | No further code needed. |
| L3 | Valid and fixed. Enabled channels require representable positive calibration; invalid disabled-channel values normalize to zero. | Keep validation in the existing legacy-to-profile conversion. |
| L4 | Correct documentation remedy. Reserved/transport-provided errors retain numeric positions; `MEASUREMENT_NOT_READY` is a live alias. | No enum removal or renumbering. |
| L5 | Correct deletion. `_jobReadBusNext`, `_jobStatus`, and `_jobHardwareEffect` had no readers and remain absent. | Keep deletion; no replacement fields. |
| L6 | Valid and fixed. Mask/Enable writes are composed from the desired profile, not the observation cache; pre-read/write/readback retains alerts. | Keep composition and verification. Fix newly discovered pre-read drift handling under M2. |
| L7 | Intended power-down retention, not a driver defect. Reconcile/recover retain power-down; applying an active profile is the wake-up path. | Keep the documented behavior. |
| L8 | Most drift was fixed; cancellation still erased observed CVRF from compatibility snapshots. Tooling/report wording also needed correction. | Remove redundant cancellation scratch clearing; update public documentation and scanner. Seven-item breakdown below. |
| F1 | Previously fixed in the common `_readMaskEnableWithTimeout()` path. Raw reads, flag reads, summation setters and latch setters all hand off consumed CVRF. | Keep the common owner; no duplicated caller fixes. |
| F2 | Incomplete. Keyword-adjacent character literals, split timing-call identifiers, and omitted `delay()` escaped the guard. | Fix the existing lexical scanner and extend its adversarial self-tests; no parser dependency. |
| F3 | All three Arduino sites are correct: automatic service, manual stepping, owner stress. The checker uses global helper/declaration counts. | Keep firmware. Correct the claim that this structural heuristic proves per-site data flow or makes every future bypass impossible. |

L8's seven original items were also checked individually:

1. Capture uptime remains in the caller's extended absolute domain, including wrap.
2. The compatibility table distinguishes two-callback verified setters from
   three-callback Mask/Enable setters and documents destructive reads.
3. `NowMsFn` documents zero health timestamps without a clock hook.
4. Comment/literal ordering was fixed; this pass closes additional lexical
   gaps. The checker does not expand macros or evaluate directives.
5. Successful `bind()` postconditions remain consistent with pending-result
   admission guards.
6. Live channel cursors and conversion-start/readiness fields are correct except
   for the cancellation case fixed here.
7. Ignored `.pio/`, generated documentation and bytecode are build output,
   not a source defect. No cache cleanup was necessary.

## Changes made in this pass

### Invalidate certainty when a read proves drift

Reproduction: initialize, change the physical Configuration register to disable
CH1, then reconcile. The first read observes the mismatch, but an address NACK on
the repair write (or cancellation before it) previously left measurement
certainty `APPLIED`, `hardwareConfigDirty()` false, and a direct CH1 read
admitted. Alert registers had the same issue.

Profile reads now compare actual values with the **retained** profile before
deciding whether the **pending** profile needs a write. If an `APPLIED` family
is disproved, it becomes `DIRTY` immediately. This also handles hardware that
already matches a pending candidate when the job is cancelled before committing
that candidate. Merely requesting a different profile leaves still-valid old
certainty intact.

Power-down Configuration pre-reads and typed Mask/Enable pre-reads follow the
same rule. Mask comparison excludes read-clear/condition flag bits. Existing
`DIRTY/UNKNOWN` evidence remains conservative. A typed Mask setter does not
claim that repairing one register re-verifies the entire alert family.

The fix reuses the existing register encoders, comparison and certainty helpers.
It adds no transfers, retries, state members, allocation, or public API.

Four new native test functions cover 64 scenarios across all eleven managed
registers, power-down, Mask/Enable pre-reads and sample cancellation. Independent
checks also exercised all 1,024 Mask/Enable status-bit combinations to ensure
ordinary flag observations do not falsely invalidate configuration.

### Retain observed readiness after cancellation

After a staged sample consumed CVRF, `cancelJob()` cleared the work record.
The compatibility poll then copied false readiness from that cleared record,
contradicting `PollJobSnapshot::conversionReady`'s observed-for-this-job contract.

Removing two redundant clears preserves the diagnostic until result consumption.
The existing reset path still clears scratch state when the result is taken or
the next job starts. Cancellation never publishes a partial sample and never
replaces the last good sample. Both staged sampling modes are covered.

### Close timing-checker lexical gaps

This legal C++ shape previously hid `millis()`:

```cpp
char open() { return'('; }
void work() { millis(); auto close = ')'; }
```

The scanner now recognizes preprocessing numbers before apostrophes, so numeric
separators remain distinct from character literals without relying on preceding
whitespace. It joins backslash-newline continuations before matching calls,
preserves raw-string opacity when a splice would manufacture a terminator, and
includes the already-prohibited `delay()` in its forbidden-call set.

Self-tests cover adjacency, decimal/binary/hex separators, fractional and
scientific/hexadecimal-float forms, prefixed literals, split calls, continued
comments, and raw bodies containing physical backslash-newline pairs. This is
a source guard with a stated lexical scope, not a C++ preprocessor.

## Other audit claims and release boundaries

The retained remedies for current precision, TCF, reset evidence, observed
Configuration, and transport statuses remain preferable to the audit's broader
proposals. The source contains no need for new configuration objects, freshness
engines, software latches, or transport abstractions.

Register formats and the successful-path transfer ceilings
**35/33/33/8/7/3** remain unchanged. Profile application remains non-atomic;
observed partial effects and application policy are the correct contract.
Initialization only disables timing control under the datasheet's actual
Configuration-write timing condition, not unconditionally.

The input audit's claim that HIL is still an unchanged 379-step suite is stale:
the current runner already has **404 default / 407 benchmark steps**, including
cancellation, TCF pairs and setter transfer counts. Physical evidence still
predates those changes.

Changes remain under `Unreleased`. Metadata is consistently `3.1.0`, the latest
published tag. The additive public changes from earlier audit passes require
the next **3.2.0 release**, with corrected TCF snapshot semantics called out.
This verification commits fixes; it does not publish or tag a release.

## Validation

| Check | Result |
|---|---|
| Baseline native suite | 151/151 passed |
| Native suite with new regressions | 155/155 passed; four added functions cover 64 scenarios |
| Independent host reproductions | Clock/wrap, setters, direction, ownership, readiness, drift and cancellation cases checked |
| Strict host warnings | Passed, including conversion/sign-conversion warnings as errors |
| Static contracts | Timing guard, Arduino CLI and IDF example checks passed |
| HIL host validation | Parser self-test and 404/407-step dry runs passed; IDs unique and final health step retained |
| Arduino ESP32-S3 / ESP32-S2 | Baseline builds passed; final local rebuild blocked by missing shared compiler executables; CI evidence pending |
| Doxygen / metadata | Warning-clean; metadata and generated version checks passed |
| Packaging | Passed; tarball written outside the repository |
| Native ESP-IDF | Final CI evidence pending; no local IDF/Docker/installed WSL |
| Physical HIL | Not run: COM11/COM12 are ESP USB interfaces, but neither was confirmed as the isolated INA3221 fixture |

Windows commands use the required VS Code-managed wrapper. The first Arduino
build encountered Windows' disabled long-path support while unpacking framework
libraries under the user path. Reusing the already installed packages at
`C:/pio/packages` resolved it without installing another PlatformIO Core or
changing system settings:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $env:USERPROFILE '.platformio'
$env:PLATFORMIO_PACKAGES_DIR = 'C:/pio/packages'
$env:PLATFORMIO_OFFLINE = '1'
.\scripts\pio.cmd run -e esp32s3dev -e esp32s2dev
```

The final local rebuild subsequently failed before source compilation because
`xtensa-esp32s3-elf-g++` and `xtensa-esp32s2-elf-g++` could not be found. PlatformIO
had refreshed the shared tool packages, and another project's PlatformIO build
was observed running concurrently. That is consistent with a shared-cache
conflict, but the precise cause was not established. No unrelated process was
stopped or shared tool installation manually repaired; CI supplies the final
embedded build check.

Native tests use the existing user-managed package directory (omit the
`PLATFORMIO_PACKAGES_DIR` override). Other checks are the three contract scripts,
`check_strict_compile.py`, `scripts/generate_version.py check`,
`check_metadata_consistency.py`, HIL parser/dry-run commands, `doxygen Doxyfile`,
`scripts/pio.cmd pkg pack`, and `git diff --check`.
