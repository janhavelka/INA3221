# Code audit resolution report

Audit reviewed: 2026-08-27 report against library `3.1.0` / `9c18102`.

Resolution branch: `main`.

Synced starting revision: `0c7e6a4`

## Outcome

Every reported finding was checked against the current implementation,
datasheet-facing contracts, native fault model, examples, and documentation.
The defects that still reproduced were fixed. Where the proposed remedy would
erase useful evidence, break compatibility, or overstate what software can
know, the smaller contract-preserving remedy is recorded below.

The datasheet register map, masks, reset values, conversion tables, and raw
decode/scaling paths remained unchanged; the audit was correct that the defects
were in lifecycle, certainty, timeout, compatibility, and documentation logic.

## Finding-by-finding disposition

| Finding | Verdict | Resolution |
|---|---|---|
| C1 | Valid | `readBlocking()` now drives the owner job with absolute extended time until the owner deadline. Its clock-stall guard resets on either time or transfer progress, and capture uptime stays in the caller's monotonic domain. Tests cover 1,000 spins/ms, a truly stalled feasible wait, and 32-bit wrap. |
| C2 | Valid | All typed Configuration mutations use one write/readback verifier. Candidate profile state commits only after a matching read; successful setters remain `APPLIED`, while failed/mismatched writes retain honest certainty. Triggered direct reads now work without a full `recover()`. |
| C3 | Valid | Rebind and lifecycle/recovery jobs bus-silently discard stale legacy conversion bookkeeping because they replace Configuration. Sample jobs still reject mixed ownership. Added `cancelConversion()` and matching Arduino/IDF CLI command. |
| H1 | Valid | Float current, power, and combined-channel reads now apply `ShuntCalibration::direction` directly, preserving float precision. |
| H2 | Valid polarity defect; proposal refined | Raw `timingControl` now means TCF high/no fault. `timingControlFault` exposes TCF low as the latest condition. It is not added to destructive event bits because TCF is a condition-level observation, not a software-created read-clear event. Documentation now states the precise, conditional timing-control disable rule for an early Configuration write. |
| H3 | Partly valid | Owner early deadline exits explicitly clear callback-invocation evidence, so a transfer that was never requested is bus-silent and cannot become indeterminate. Callback invocation alone cannot prove a physical transfer occurred inside an adapter, so phase-definite address NACK/not-found and adapter validation statuses remain non-ambiguous; adapters are required to return accurate phase/status information. |
| H4 | Valid | Shunt-sum reads now require a shunt-measuring mode, at least one selected summation channel, and verified alert configuration, all before I2C. A dormant summation selection remains valid in a bus-only/power-down desired profile because rejecting it would unnecessarily prevent retained future configuration. |
| H5 | Valid | The Arduino example caps an explicit transfer budget to `remaining / fixed Wire timeout` and pauses bus traffic when no callback can fit. Its unsupported tighter-timeout preflight is `INVALID_CONFIG`, not a fabricated bus timeout. Native IDF retains true per-call timeout support. |
| M1 | Valid dual-source risk; breaking proposal rejected | `_transport` and `_profile` are authoritative for core behavior. The retained `Config` object is now explicitly a legacy compatibility/observed view, preserving public ABI and raw-register diagnostics without allowing core policy to read it. Raw Configuration writes do not destroy the desired profile needed by reconcile/recovery. |
| M2 | Valid | Typed alert limits, summation/latch settings, and Configuration setters share the managed write/readback verifier. Candidate values and generation changes commit only after verification. |
| M3 | Valid | A central failure terminalizer consistently maps `DEADLINE_EXPIRED` to `TIMED_OUT`, distinguishes ambiguous invoked writes, and reports `PARTIAL` only after a confirmed earlier write. An interior zero-share deadline test proves zero callbacks and `HardwareEffect::NONE`. |
| M4 | Valid | Tracked initialization transfers now update success/failure counters and timestamps before initialization; only `DriverState` transitions remain guarded by `_initialized`. Failed first bring-up diagnostics survive unbind. |
| M5 | Valid fabricated-observation defect; proposed clear rejected | Software reset no longer fabricates a zero Mask/Enable reading. It resets only known hardware/configuration caches and leaves real host-retained alert history untouched until the application takes it. Clearing the entire snapshot, as proposed, would silently acknowledge previously observed events. |
| M6 | Valid | CVRF-low fault polling uses a fixed 50 ms recheck instead of roughly 2 ms. When another read cannot fit, the job waits bus-silently for the absolute deadline. |
| M7 | Valid | The native example now owns `components/INA3221/CMakeLists.txt`, a stable-name shim referencing root source/include paths. It no longer derives the component name from the checkout directory. |
| L1 | Valid | Float-domain saturation occurs before `lrintf`; tests cover maximum/lowest finite floats for shunt and bus encoders. |
| L2 | Valid | Removed unreachable negative guards after masked power-valid decoding. |
| L3 | Valid | Legacy conversion accepts missing/invalid shunt values on disabled channels and normalizes them to zero; enabled channels still require finite positive calibration. |
| L4 | Documentation drift only | Kept enum values for source compatibility. `BUSY` and `DEVICE_OFFLINE` are documented as reserved; timeout/I2C categories are documented as transport-supplied, and `CONVERSION_NOT_READY` remains an actively produced compatibility result. |
| L5 | Valid | Removed the owner members that were assigned but never read. |
| L6 | Valid | Mask/Enable setters now compose the complete desired register through the existing profile encoder, not the latest destructive-read cache, and verify readback. |
| L7 | Valid documentation gap | Public API and README now state that successful power-down becomes the retained desired mode and reconcile/recover preserve it. |
| L8 | Valid | Corrected absolute capture uptime, callback-count/readiness documentation, `nowMs` health usage, and the timing-checker string/comment stripping order. The previously reported ignored tarball and stale generated Doxygen directory were absent from the synced worktree; Doxygen was then freshly regenerated. Snapshot cursor fixes from the audit pass are now exercised alongside the expanded native suite. |

## Additional review corrections

- The audit's "already fixed" raw-write Doxygen wording still needed a reset
  exception; confirmed reset is `DIRTY`, while an ambiguous reset is `UNKNOWN`.
- The repository engineering guide overstated that every compatibility method
  is layered on the owner job engine. It now distinguishes owner-backed
  lifecycle/blocking/staged work from direct tracked synchronous calls.
- Configuration-first profile application is safe at power-on defaults but a
  live apply is not atomic. The README now exposes that transient instead of
  generalizing the power-on safety argument.
- Alert evidence, timing-control level/fault, power-down retention, managed
  setter verification, fixed-timeout budget admission, and health provenance
  are printed or documented consistently across both example stacks.

## Validation

Native coverage increased from the synced baseline of 124 tests to 133 tests.
The completed local matrix was:

- `python tools/check_cli_contract.py` — passed;
- `python tools/check_idf_example_contract.py` — passed;
- `python tools/check_core_timing_guard.py` — passed;
- `python tools/check_metadata_consistency.py` — passed (`3.1.0`);
- `python scripts/generate_version.py check` — passed;
- `python tools/check_strict_compile.py` — passed;
- `python tools/hil_cli_runner.py --parser-self-test` — passed;
- `doxygen Doxyfile` — passed with no warnings;
- `.\scripts\pio.cmd test -e native` — 133/133 passed;
- `.\scripts\pio.cmd run -e esp32s3dev` — passed;
- `.\scripts\pio.cmd run -e esp32s2dev` — passed.

Native ESP-IDF compilation was not run locally because `idf.py` was not
installed. The static IDF source contract passed, but is not presented as a
compiler/linker result. Hardware HIL was also not run; only its parser
self-test was executed.
