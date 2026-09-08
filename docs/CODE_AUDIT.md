# INA3221 audit closeout

The 2026-08-27 audit covered `3.1.0` at `9c18102`; independent reviews on
2026-09-05 and 2026-09-08 closed **23 original findings and 3 follow-ups**.
This consolidated record starts from clean, synchronized `main` at `8b89b4f`.
It retains the finding IDs, corrected audit claims, and verification provenance
that the product documentation does not contain. Permanent contracts and design
rationales live in the [README](../README.md), [changelog](../CHANGELOG.md), and
[public headers](../include/INA3221/INA3221.h).

The device reference is TI SBOS576C, especially sections 6.5, 7.3.2.4 and 7.6.2,
available as the [bundled datasheet](INA3221_datasheet.pdf). Reviews found no
register-map, scaling, reset-default, conversion-table, or successful-path
callback-ceiling change necessary. Historical reports remain in Git history;
they are superseded by this record.

## Finding status and verification

"Fixed" below means implemented and software-verified; physical qualification
is separate. Test names refer to the existing [basic](../test/test_basic.cpp)
and [owner](../test/test_owner_operations.cpp) suites; implementations remain in
[INA3221.cpp](../src/INA3221.cpp). Run them with `scripts/pio.cmd test -e native`.

| ID | Finding / current disposition | Implemented remedy | Verify by |
|---|---|---|---|
| C1 | Iteration-bounded blocking reads: fixed. | `readBlocking()` uses extended absolute time and a progress-aware stalled-clock guard. | `test_read_blocking_tolerates_many_spins_per_millisecond`, `test_read_blocking_stall_guard_cancels_feasible_wait`; 18 independent clock/profile/wrap cases. |
| C2 | Typed setters blocked subsequent measurements: fixed. | Shared write/readback verification commits the candidate only after a matching Configuration read. | `test_typed_config_setter_requires_matching_readback`; 26 independent setter/read combinations. |
| C3 | Stale legacy conversions blocked recovery: fixed. | Lifecycle/rebind admission discards legacy bookkeeping; sample jobs reject mixed ownership; `cancelConversion()` is bus-silent. | `test_legacy_conversion_can_be_cancelled_or_abandoned_for_recovery`, `test_successful_rebind_discards_active_legacy_conversion_bus_silently`. |
| H1 | Legacy current direction was ignored: fixed. | Float current/power/channel readers apply the profile sign; `Config::direction` round-trips. | `test_float_measurement_helpers_apply_profile_direction`, `test_legacy_config_current_direction_round_trips_through_profile`; fractional-current checks on all channels. |
| H2 | Sticky TCF prevented reporting its low level: fixed; original polarity claim rejected. | Retain the latest raw level and expose its inverse as `timingControlFault`; no second latch. | `test_owner_alert_events_are_retained_and_taken_exactly_by_owner`; HIL parser tests accept both complementary level/fault pairs. |
| H3 | Failure classification confused callbacks with physical attempts: fixed within the adapter contract. | `_lastCallbackInvoked` distinguishes driver rejection; adapter-local validation explicitly promises no backend access. | `test_owner_interior_deadline_is_bus_silent_and_terminalizes_timed_out`; deadline-after-read reproduction gives `TIMED_OUT/NONE` without a write callback. |
| H4 | Invalid summation reads returned stale values: fixed; freshness limitation documented. | Require shunt mode, selected channels, and verified alert state before I2C; changed selection still needs a completed cycle. | `test_shunt_sum_rejects_bus_only_modes_without_i2c`; empty-selection/unknown-state host checks and HIL `DATA-010B`/`DATA-010C`. |
| H5 | Wire rejected divided deadlines as bus timeouts: fixed. | Reject unsupported bounds as `INVALID_CONFIG`; cap all Arduino owner budgets with `wireSafeTransferBudget()`. IDF keeps per-call timeouts. | `check_cli_contract.py`; actual helper passed 3,328,256 budget/deadline combinations plus boundary cases; HIL `OWNER-026A`. |
| M1 | Configuration caches disagreed: resolved with an observed view. | `_legacyConfigView` describes observed Configuration and drives legacy timing; `_profile` remains desired state. | `test_raw_config_view_survives_host_updates_and_drives_readiness`; `writeConfig(0x4FFB)` causes no premature readiness read at 100 ms. |
| M2 | Alert setters lost certainty; later reads could retain false `APPLIED`: fixed. | Verify typed writes without promoting the whole alert family. Profile, power-down, and Mask/Enable pre-reads mark observed retained-profile drift `DIRTY` before repair. | `test_typed_alert_setter_requires_matching_readback`, `test_owner_profile_reads_disprove_retained_certainty_before_writes`, `test_typed_mask_pre_read_disproves_certainty_without_losing_alerts`. |
| M3 | Interior deadlines reported `FAILED`: fixed. | `_finishJobFailure()` centralizes `TIMED_OUT` classification independently of partial/indeterminate effects. | `test_owner_interior_deadline_is_bus_silent_and_terminalizes_timed_out`, `test_owner_triggered_interior_deadline_is_partial_but_keeps_config_applied`; cancellation/failure matrices. |
| M4 | Failed initial bring-up lacked telemetry: fixed. | Tracked transfers always update counters/errors/timestamps; only state transitions require initialization. | Failing-first-`begin()` reproduction retains `I2C_BUS`, its timestamp, and one failure while state remains `UNINIT`; initialization tests. |
| M5 | Reset fabricated alert evidence: fixed with a narrower remedy. | Update known reset defaults without clearing untaken events or inventing a consuming-read snapshot. | `test_soft_reset_preserves_real_retained_alert_evidence`, `test_ambiguous_reset_preserves_observed_mask_enable_cache`. |
| M6 | CVRF-low fault polling flooded the bus: fixed. | Use fixed 50 ms rechecks and bus-silent waiting when another bounded read cannot fit; preserve initial conversion-start time. | `test_owner_permanently_low_cvrf_waits_until_owner_deadline`, `test_legacy_single_shot_snapshot_keeps_initial_start_during_cvrf_recheck`. |
| M7 | Native IDF depended on checkout basename: fixed. | Existing fixed-name component shim resolves root sources; CI mounts `/component-source`. | `check_idf_example_contract.py`; both native-IDF CI targets. |
| L1 | Out-of-range floats reached `lrintf()`: fixed. | Clamp finite values before conversion; retain documented zero result for nonfinite input. | `test_volts_to_bus_raw_boundaries` and independent extreme/NaN/infinity checks. |
| L2 | Negative power-valid guard was unreachable: removed. | Retain writable mask `0x7FF8`; delete the impossible post-mask negative branch. | `test_alert_limit_writes_clear_reserved_bits`; inspect masked power-valid setters. |
| L3 | Disabled channels required valid calibration: fixed. | Validate representable positive resistance only for enabled channels; normalize invalid disabled values to zero. | `test_begin_ignores_invalid_shunts_on_disabled_channels`; independent zero/negative/nonfinite/submicroohm/oversized cases. |
| L4 | Unproduced public error values: documented, retained. | Preserve reserved and transport-supplied numeric values and the live `MEASUREMENT_NOT_READY` alias. | `Status.h` annotations and enum history; no removal or renumbering. |
| L5 | Owner fields were written but never read: removed. | Delete `_jobReadBusNext`, `_jobStatus`, `_jobHardwareEffect`; no replacement state. | Search current core/header; historical read-site inspection. |
| L6 | Mask/Enable writes used an observation cache: fixed. | Compose desired writable bits from `_profile`; shared pre-read/write/readback retains alerts and detects drift. | `test_raw_cached_register_write_and_reset_remain_dirty_until_recover`, `test_mask_enable_setter_failed_pre_read_prevents_write`; M2 regressions. |
| L7 | Power-down became retained desired state: intended behavior, documented. | Reconcile/recover retain power-down; apply a measurement profile to wake. | `test_power_down_returns_status_and_keeps_driver_initialized`; power-down owner tests and public lifecycle contract. |
| L8 | Seven smaller contract gaps and later snapshot defects: fixed/documented. | Preserve absolute capture time; correct callback/read-clear and clock docs; repair lexical guard; clarify successful `bind()` postconditions; publish live/stable sample diagnostics, including cancellation; classify ignored output as build artifacts. | README compatibility table, `NowMsFn`/`bind()` Doxygen, F2 self-tests, wrap tests, `test_legacy_sample_snapshot_retains_cvrf_before_and_after_channel_reads`, `test_legacy_cancelled_sample_snapshot_keeps_observed_ready`; no cache deletion needed. |
| F1 | Consuming Mask/Enable reads lost legacy CVRF: fixed across every read path. | `_readMaskEnableWithTimeout()` retains events and hands readiness to legacy state before returning, including verifier mismatches. | `test_typed_mask_enable_setters_preserve_observed_conversion_ready`, `test_raw_mask_enable_read_preserves_observed_conversion_ready`, pre-read failure/event tests. |
| F2 | Timing guard could hide forbidden calls: fixed within lexical scope. | Recognize numeric tokens, prefixed/adjacent literals and line splices; preserve raw-string opacity; reject `delay()` too. | `check_core_timing_guard.py` adversarial self-tests; GCC confirmed reproductions are valid C++17. No macro expansion or directive evaluation is claimed. |
| F3 | One of three Arduino poll sites lacked the Wire cap: fixed. | Automatic service, manual stepping, and owner stress all use the same helper. | Inspect all three `PollContext` sites; CLI contract's global count heuristic and HIL `OWNER-026A`. |

## Corrections to the audit

- **Scope/count:** there are 23 original findings, not 22, plus F1-F3. Earlier
  blanket closure claims missed raw-read CVRF handoff, the required Mask/Enable
  pre-read, snapshot failures/cancellation, observed-profile drift, and lexer
  gaps. The table includes the final remedies.
- **H2:** raw TCF polarity was already correct. POR TCF is 1; sticky OR hid a
  subsequent low level. A software fault latch would duplicate the device's
  latch. Initialization does not unconditionally disable TC: that depends on
  whether/when Configuration is actually written.
- **M5:** clearing the entire retained snapshot would silently acknowledge
  untaken events. Retaining real evidence is the correct reset policy.
- **M1/H1:** deleting the legacy view would discard reference-returning API and
  raw-diagnostic semantics; routing float current through whole milliamps would
  lose fractional precision. The implemented remedies preserve both.
- **H3:** callback invocation cannot reveal the adapter's internal transfer
  phase. Validation statuses require the adapter's bus-silent guarantee.
- **M2/L6:** one verified alert register cannot prove all ten registers match.
  Raw writes already exposed uncertainty, so the original "silent clobber"
  framing was overstated. Observed drift must invalidate prior certainty even
  if cancellation or a non-reaching write failure prevents repair.
- **L7:** retained power-down is an intentional lifecycle contract, not a defect.
- **M1 arithmetic:** raw `0x4FFB` needs 16.9 s typical / 18.6 s maximum, not
  1.24 s; the erroneous old 8 ms gate was real.
- **C1 arithmetic:** AVG_1024 with default 1.1 ms conversion settings needs
  about 7.4 s maximum. The 55.7 s maximum additionally requires 8.244 ms
  conversion settings; the default AVG_1/AVG_4 waits remain about 8/30 ms.
- **C1 HIL reference:** `OWNER-037 mode sbc` preceded the old aggregate `read`
  checks; `DATA-003A` followed them. Those historical checks did not exercise
  triggered blocking waits. Current HIL includes AVG_1/AVG_4 triggered reads.
- **L1 range:** the 32-bit-`long` threshold is about +/-85,900 V, not +/-85 V.
  **L4:** `MEASUREMENT_NOT_READY` aliases a produced error. **L5:** the removed
  status/effect members had no readers, including through the pending result.
- **Non-defects:** successful `bind()` postconditions do not bypass pending-result
  admission; trigger-fit rejection occurs before I2C on the first poll;
  Configuration-before-alert profile application is observable and non-atomic,
  not proven glitch-free by POR defaults. Native IDF's general NACK-error mapping
  preserves an otherwise unknown phase, as documented in [IDF_PORT.md](IDF_PORT.md).
- **Tooling/evidence:** F3's helper-count check is a structural heuristic, not
  per-site data-flow proof. The timing guard is lexical, not a preprocessor.
  Ignored build output is not a source defect. Historical line citations were
  stale; use the symbols/tests above. Current HIL has 404 default / 407 benchmark
  steps, including cancellation, complementary TCF levels and setter counts;
  neither parser success nor a dry run is hardware evidence.

## Verification provenance

The first fixes were `8dff5f9`, `a41e791`, and `1eb94c7`. The 2026-09-05
follow-up reached **151/151 native tests** and passed complete
[CI 33986107170](https://github.com/janhavelka/INA3221/actions/runs/33986107170)
at implementation commit `2ffa66afeb41108a319988c7e5e36dbe3890be4f`.

The 2026-09-08 review reached **155/155 native tests**; four additional test
functions cover 64 drift/cancellation scenarios. Independent checks included
all 1,024 Mask/Enable status-bit combinations. Complete
[CI 34210620245](https://github.com/janhavelka/INA3221/actions/runs/34210620245)
passed for implementation commit `2c5240b29561c4d438925305e8dd182c8d85b318`,
covering native/strict checks, both Arduino targets, packaging/documentation/
contracts, and both IDF 6.0.1 targets. The closeout baseline `8b89b4f` also passed
[CI 34211106830](https://github.com/janhavelka/INA3221/actions/runs/34211106830).

Earlier local Arduino rebuilds encountered disabled Windows long-path support
and later unavailable shared Xtensa tools; the exact latter cause was not
established. Those reports relied on the corresponding successful CI builds,
not failed local runs. Historical review reports and recorded validation results
remain in Git history rather than being duplicated here; raw HIL transcripts are
external.

## Current closeout and open items

Closeout started on **2026-09-08** from `8b89b4f`. This file replaces the two
reports and the documentation index points here; neither audit file is a
Doxygen `INPUT`. Repeated synchronous setters deliberately retain full
write/verification costs: the shared Configuration path also performs explicit
triggers, for which an identical write can start a new conversion. The
cooperative engine's conditional writes serve a different operation contract.

- **Release 3.2.0: complete.** Release commit
  `39f28767b346211dda320286166490b4fdf8b91f` passed
  [CI 34232799942](https://github.com/janhavelka/INA3221/actions/runs/34232799942)
  before the annotated `v3.2.0` tag was created and pushed. The
  [GitHub release](https://github.com/janhavelka/INA3221/releases/tag/v3.2.0)
  is published with the TCF migration note and explicit HIL limitation, following
  [CONTRIBUTING.md](../CONTRIBUTING.md).
- **Current local validation: PASS.** Native 155/155; strict host warnings;
  timing, Arduino CLI and IDF contracts; generated version and 3.2.0 metadata;
  HIL parser self-test and 404-step dry run; warning-clean Doxygen; both Arduino
  ESP32-S3/ESP32-S2 builds; package creation; and `git diff --check`.
  **Native IDF local builds: NOT RUN** (no local IDF/Docker/installed WSL).
  The release commit's full CI matrix passed, including both native IDF 6.0.1
  targets built from `/component-source`. This subsequent documentation update
  records the release evidence; no implementation changed after that passing run.
- **Physical HIL: NOT RUN.** No isolated INA3221 fixture is confirmed; COM11/COM12
  identify ESP USB interfaces only. The hardware workflow stops at this boundary.
  [HIL.md](HIL.md) retains the actual evidence ledger and required qualification
  procedure. Its 379-step release evidence predates every audit fix. Real-device
  setter/read-clear timing, CVRF fault rechecks, and observed-drift handling remain
  unqualified by the expanded suite; no result or transcript hash is inferred.

The current Windows build reused installed packages with process-only settings:
`PLATFORMIO_CORE_DIR=$USERPROFILE/.platformio`,
`PLATFORMIO_PACKAGES_DIR=C:/pio/packages`, `PLATFORMIO_OFFLINE=1`, and a `PATH`
prefix of `$USERPROFILE/.platformio/tools/toolchain-xtensa-esp-elf/bin`
(toolchain `14.2.0_20260121`). All PlatformIO commands used `scripts/pio.cmd`;
no additional Core installation or shared-tool modification was needed.
