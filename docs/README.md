# Documentation

The library contract lives in the [project README](../README.md) and public
headers under [`include/INA3221/`](../include/INA3221/INA3221.h). This directory
contains maintained integration guidance and the primary device datasheet.

## Maintained guides

- [Native ESP-IDF integration](IDF_PORT.md) documents transport ownership,
  deadlines, cooperative jobs, and the native example.
- [Hardware-in-the-loop validation](HIL.md) defines the bounded fixture run,
  coverage boundary, and reviewed evidence ledger.
- The <a href="INA3221_datasheet.pdf">INA3221 datasheet</a> is the authoritative
  bundled device reference. Register and behavior contracts implemented by the
  library are documented in the public headers and tests rather than duplicated
  in generated research notes.

## Audit record

- [Retained branch review](RETAINED_BRANCH_AUDIT.md) inventories historical
  audit/port tips, reconciles their findings with the reviewed source, and records the
  PV contract resolution and remaining physical-qualification/timing questions.
- [Audit closeout](CODE_AUDIT.md) maps all 26 findings to their final remedies
  and verification, preserves corrections to the original audit and CI provenance,
  and records the release and hardware-validation boundary. Permanent contracts
  remain in the README, changelog and public headers.

Completed implementation prompts have been removed from the working tree;
their instructions remain in Git history and their resolution commits/checks
are recorded in the retained branch review. TIM-1 (earliest CVRF timing) and
HIL-1 (physical qualification) remain open. The historical 3.1.0 HIL results
do not qualify the current 3.2.0 implementation.

## Generated and validation output

Doxygen HTML is generated under `docs/doxygen/` and is ignored by Git. Generated
HIL reports, serial transcripts, logs, and other run-specific artifacts belong
in a temporary or external directory. The compact reviewed evidence ledger and
reproducible commands are maintained in [HIL.md](HIL.md); release-relevant
behavior changes belong in the [changelog](../CHANGELOG.md).
