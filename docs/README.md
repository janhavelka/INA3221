# Documentation

The library contract lives in the [project README](../README.md) and public
headers under [`include/INA3221/`](../include/INA3221/INA3221.h). This directory
contains maintained integration guidance and the primary device datasheet.

## Maintained guides

- [Native ESP-IDF integration](IDF_PORT.md) documents transport ownership,
  deadlines, cooperative jobs, and the native example.
- [Hardware-in-the-loop validation](HIL.md) defines the bounded fixture run,
  coverage boundary, and reviewed evidence ledger.
- [Code audit resolution](CODE_AUDIT_RESOLUTION.md) records the disposition of
  every 2026-08-27 audit finding and the remedies selected after verification.
- The <a href="INA3221_datasheet.pdf">INA3221 datasheet</a> is the authoritative
  bundled device reference. Register and behavior contracts implemented by the
  library are documented in the public headers and tests rather than duplicated
  in generated research notes.

## Generated and validation output

Doxygen HTML is generated under `docs/doxygen/` and is ignored by Git. Generated
HIL reports, serial transcripts, logs, and other run-specific artifacts belong
in a temporary or external directory. The compact reviewed evidence ledger and
reproducible commands are maintained in [HIL.md](HIL.md); release-relevant
behavior changes belong in the [changelog](../CHANGELOG.md).
