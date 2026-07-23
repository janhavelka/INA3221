# Documentation

The library contract lives in the [project README](../README.md) and public
headers under [`include/INA3221/`](../include/INA3221/INA3221.h). This directory
contains maintained integration guidance and bundled source material.

## Maintained guides

- [Native ESP-IDF integration](IDF_PORT.md) documents transport ownership,
  deadlines, cooperative jobs, and the native example.
- <a href="INA3221_IMPLEMENTATION_MANUAL.md">INA3221 chip implementation
  manual</a> consolidates chip behavior and source ambiguities. It is a chip
  reference, not the library API contract.
- <a href="extracted-md/00_document_inventory.md">Compact datasheet notes</a>
  provide a focused map of registers, transactions, timing, alerts, and
  operating modes.

## Bundled source material

- <a href="INA3221_datasheet.pdf">INA3221 datasheet</a> is the primary device
  source.
- <a href="application_notes/">Application notes</a> and
  <a href="reference/">supplemental references</a> contain vendor PDFs and
  concise notes.
- <a href="pdf-extracted-md/">Markdown extractions</a> contain full PDF
  transcripts.
- <a href="pdf-extracted-txt/">Plain-text extractions</a> contain the
  corresponding raw transcripts.

Both extraction formats are intentionally retained. Their different structure
and pagination make them useful for source checking and AI-assisted coding.

## Generated and validation output

Doxygen HTML is generated under `docs/doxygen/` and is ignored by Git. HIL
reports, serial transcripts, logs, and other run-specific evidence are not
maintained as repository documentation; write them to a temporary or external
artifact directory. Release-relevant behavior changes belong in the
[changelog](../CHANGELOG.md), while reproducible validation commands belong in
the project README and CI configuration.
