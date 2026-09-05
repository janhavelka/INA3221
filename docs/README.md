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

## Transient review artifacts

- [Independent audit verification](CODE_AUDIT_VERIFICATION.md) records the
  2026-09-05 review of every finding, additional fixes, and validation evidence.
- [Code audit](CODE_AUDIT.md) records the 2026-08-27 audit of `3.1.0`, the
  resolution of every finding, and how to re-verify each one. Every permanent
  contract change it describes is already in the changelog, the README and the
  public headers, so the file may be deleted once an independent reviewer has
  confirmed the implementations.

## Generated and validation output

Doxygen HTML is generated under `docs/doxygen/` and is ignored by Git. Generated
HIL reports, serial transcripts, logs, and other run-specific artifacts belong
in a temporary or external directory. The compact reviewed evidence ledger and
reproducible commands are maintained in [HIL.md](HIL.md); release-relevant
behavior changes belong in the [changelog](../CHANGELOG.md).
