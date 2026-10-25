# 01 — Make the power-valid integration contract complete and usable

Address **PV-1** from [the retained-branch review](../RETAINED_BRANCH_AUDIT.md#pv-1-explain-when-the-pv-signal-is-meaningful).
Read [the prompt index](README.md) and [AGENTS.md](../../AGENTS.md). Re-audit the
current implementation and authoritative device text, then deliver the
documentation/contract correction with proportionate checks and a commit.
This is not an instruction to implement an unrelated power-management layer.

## Provenance and mechanism

Source baseline reviewed 2026-10-25:
`40838b56e41c25183478b086e9e214ceffe3290c` on `main`.
The historical finding comes from report-only commit
`70e563301dd85845c3a20a6d91d10f8decb43b59`, followed by prompt-only tip
`2b3093b7de04e6a0c4cff4c604e125c1dcd9dc62` on
`origin/audit/ina3221-industry-readiness-exploration`. There is no lost code
implementation to cherry-pick from those commits.

Current `_validateProfile()` validates mode/channel/calibration and threshold
fields, then delegates PV-window validation to `validatePowerValidWindow()`.
That helper checks ordering, representability and the 26 V limit. It does not
and cannot establish external wiring, fresh bus samples, or PV-output voltage.
`AlertFlags::powerValid` and `AlertSnapshot::powerValid` expose the observed PVF
condition. They do not certify a freshly measured board-level power-good state.

Consequently, an application can configure an encodable window and observe
PVF yet use it incorrectly when bus conversions are disabled/stale or unused
inputs are unsuitable. The threshold encoder is not proven defective. The
missing piece is a clear integration contract at the public point of use.

## Read and reconcile these exact owners

Read the live code and comments, not just the retained report:

- `include/INA3221/Config.h`: `Mode`, `AlertProfile`, `DeviceProfile`,
  `powerValidUpperMilliVolts`, `powerValidLowerMilliVolts`, channel enable mask.
- `include/INA3221/INA3221.h`: `AlertFlags`, `AlertSnapshot`,
  `validatePowerValidWindow`, the four upper/lower setters/getters,
  `readAlertFlags`, and retained-alert peek/take contracts.
- `src/INA3221.cpp`: `_validateProfile`, `validatePowerValidWindow`,
  `_desiredRegister`, `_retainMaskEnable`, `readAlertFlags`, PV setters and
  getters. Confirm condition-level PVF remains distinct from retained
  read-clear CF/SF/WF/CVRF events and configuration certainty.
- `README.md`: device profile/example, alert evidence, timing and integration
  contracts. The current example explains shunt calibration but does not supply
  the PV qualification conditions.
- `test/test_owner_operations.cpp`: full-profile apply/readback tests,
  `test_owner_alert_events_are_retained_and_taken_exactly_by_owner`,
  `test_owner_fixed_unit_helpers_cover_boundaries_reverse_and_overflow`, and
  `test_owner_wait_uses_maximum_timing_and_crosses_uint32_boundary_without_io`.
- `test/test_basic.cpp`: typed alert setter verification, Mask/Enable
  read-clear retention, and direct/raw compatibility contracts.
- `docs/CODE_AUDIT.md`, `docs/HIL.md`, `docs/IDF_PORT.md`, `docs/README.md`,
  `.github/workflows/ci.yml`, `Doxyfile`, and the version/metadata scripts.

Use `rg -n 'powerValid|PowerValid|PVF|maximumCycleTimeUs' include src README.md test`
and inspect each relevant body. Verify names against current source after
fetching; do not reset to the recorded baseline or overwrite newer work.

## Device authority and facts to express accurately

Re-read [the bundled TI SBOS576C datasheet](../INA3221_datasheet.pdf),
**section 7.3.2.3, printed pages 13–14**, including Figures 7-1 and 7-2.
The section number matters: 7.3.2.4 is **Timing-Control Alert**, a different
function. This citation was checked directly during prompt preparation.
Use section **7.6.2.16**, Table 7-36 on printed page 34, as the authority for
Mask/Enable/PVF polarity and persistence. It identifies PVF as the observed PV
pin level and distinguishes it from the read-clear event flags. Do not infer
polarity merely from the C++ member name.

The public guide must accurately cover:

1. PV upper/lower hysteresis and startup state. The default 10 V/9 V thresholds
   are register defaults, not universal safe settings for every board.
2. Bus-voltage conversions must run periodically for PV state to reflect rail
   changes. A stored condition can become stale in power-down, shunt-only or
   an application that does not schedule subsequent triggered conversions.
   Do not claim a typed threshold setter starts a monitoring service.
3. The datasheet narrative requires all three monitored rails and specifies
   wiring unused-channel IN- to a used rail, leaving unused IN+ floating.
   Figure 7-1 says "enabled channel"; preserve that context and the explicit
   unused-input guidance. Do not infer that clearing a software channel mask
   alone makes arbitrary floating inputs valid for PV.
4. PV output level depends on VPU and the documented output structure. Explain
   that board voltage/level compatibility must be checked; an I2C read or a
   threshold range check cannot prove it. Refer to the actual electrical
   ratings instead of inventing resistor values or assuming a GPIO-safe level.
5. The distinction between raw observed PVF, threshold configuration and an
   application decision to trust power-good. Specify update/freshness and
   application ownership in plain language, without adding hidden policy.

Keep TC and PV guidance separate. Do not alter TC initialization behavior or
invent an automatic TC-preserving lifecycle as part of this correction.

## Implementation direction

1. Add one concise, source-cited PV integration section to the existing README
   next to alert evidence/configuration certainty. Avoid duplicating the whole
   datasheet, creating another authoritative guide or resurrecting old reports.
2. Make public field/helper/setter documentation accurately state what is
   validated and what remains application/board responsibility. Add short
   Doxygen notes or references at the named API points; keep units, valid range,
   side effects and observed-condition semantics consistent.
3. Preserve valid shunt-only, powered-down and partially enabled profiles.
   Every profile carries PV thresholds even when a consumer does not use PV.
   Rejecting those profiles globally would impose a new, unjustified contract.
4. Prefer documentation only when that fully closes PV-1. If a real existing
   consumer makes an incorrect decision from PVF, fix that specific composition
   using the current profile/result/evidence API and add its regression. Do not
   add a generic `isPowerGood()` returning false confidence, a freshness timer
   inside the driver, new transport reads, or a new optional helper without a
   concrete caller.
5. If a separate code defect is actually reproduced, record its trigger and
   isolate the smallest coherent correction in the existing owner/module.
   Preserve desired/applied/unknown state, terminal result lifetime, fixed
   memory, single-attempt transport, deadlines and read-clear evidence.

## Acceptance and verification

A reader should be able to answer all of these from the maintained contract:

- Does an ordered PV window validate my wiring or current rail state? No.
- Can a shunt-only profile remain valid when PV is not used? Yes.
- Can an old PVF observation be treated as current after measurements stop?
  No; explain the observation/cadence requirement without claiming new metadata.
- How do unused inputs and VPU/PV affect safe use? The guide cites the actual
  wiring/output conditions, and makes board responsibility explicit.
- Do reading alerts or reconfiguring the device have other effects? Keep the
  existing destructive-read retention and configuration certainty guidance.

For documentation-only work, review the cited PDF, validate local links and
Doxygen, and run the existing relevant contract checks:

```powershell
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
python tools/check_metadata_consistency.py
doxygen Doxyfile
git diff --check
```

Do not write a test whose only assertion is that a sentence exists. If runtime
profile/alert/consumer behavior changes, add meaningful tests in the existing
registered suite for the affected behavior, including success, unsupported use,
read/transport failures, old event retention and no unintended I2C. Run
`.\scripts\pio.cmd test -e native` and `python tools/check_strict_compile.py`.
Retain the existing equal-window, out-of-range and encoding/profile tests.
If runtime behavior changes, add missing relevant ordering/quantization cases;
do not replace that coverage with tests for a new blanket mode rejection.
The current helper checks requested lower < upper before encoding; do not
claim it guarantees different quantized register values.

For substantial behavior/integration changes reuse the actual CI workflow:
Arduino `esp32s3dev`/`esp32s2dev`, native tests/strict compile, metadata and
contracts, HIL parser self-test, Doxygen, packaging, and native IDF 6.0.1 S2/S3.
Verify terminal CI results for the exact final commit when the change warrants
remote CI. The current workflow has no manual dispatch trigger; do not add one
just for documentation or bypass existing checks. Use the repository PlatformIO
wrapper on Windows and never install a second Core.

## Preserve the timing-evidence boundary

TIM-1 is not a request to remove the averaging multiplier from
`maximumCycleTimeUs()`. Re-read the current conservative admission contract and
the cited converter/CVRF descriptions if wording intersects with PV freshness.
An algebraic calculation or fake conversion-ready sequence does not prove the
silicon's earliest ready time. Keep the present timing unless authoritative
evidence establishes a discrepancy; record the unresolved evidence question
instead of speculative optimization. No hardware run is required or claimed
for this documentation task, and HIL-1 remains separate.

## Closeout and synchronization

Update PV-1 in `docs/RETAINED_BRANCH_AUDIT.md` and the prompt index with exact
resolution commit and checks. Record the corrected primary citation and any
remaining consumer-specific condition. Keep physical proof and TIM-1 labelled
open. Update release-facing notes if appropriate; do not manufacture a release
or edit generated version files by hand. Stage only owned changes, commit,
push without force and verify upstream synchronization under `AGENTS.md`.
