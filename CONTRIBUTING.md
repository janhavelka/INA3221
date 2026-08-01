# Contributing

Contributions are welcome when they preserve the library's deterministic,
framework-neutral embedded boundary. Read the repository's `AGENTS.md` before
changing public API or core behavior; it contains the binding engineering
rules.

## Development setup

Required for the full host/documentation pass:

- Python 3
- PlatformIO
- a C++17 host compiler
- Doxygen

ESP-IDF `6.0.1` is additionally required to reproduce the native example builds.
Hardware-in-the-loop checks require a supported ESP32-S2/S3 fixture and are not
implied by a successful host build.

Create a focused branch, make the smallest coherent change, and preserve any
unrelated worktree changes. `library.json` is the version source of truth;
`include/INA3221/Version.h` is generated and must not be edited manually.

## Engineering expectations

- Keep public headers in `include/INA3221/` and implementation in `src/`.
- Keep Arduino/ESP-IDF headers and board-specific pins out of the library core.
- Preserve application ownership of I2C, deadlines, retries, recovery, and
  serialization.
- Keep steady-state paths allocation-free, bounded, observable, and free of
  library logging.
- Return `Status` for fallible operations; do not add exceptions or silent
  fallback behavior.
- Follow the tracked `.clang-format` and the existing naming conventions.
- Add or update tests for behavior changes, including failure and boundary
  cases where relevant.
- Document every public symbol and parameter in Doxygen.
- Add a concise entry under `[Unreleased]` in [CHANGELOG.md](CHANGELOG.md).

Do not add speculative managers, services, portability layers, fake production
devices, or dependencies without a concrete current caller and a clear reason.

## Validation

Run the checks relevant to the change. A full local pass is:

```bash
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python tools/check_core_timing_guard.py
python tools/check_metadata_consistency.py
python scripts/generate_version.py check
python tools/check_strict_compile.py
python tools/hil_cli_runner.py --parser-self-test
doxygen Doxyfile
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
```

When ESP-IDF is installed, also run both native targets:

```bash
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

Report unavailable toolchains as `NOT RUN`; do not treat a static contract check
as compiler, linker, or hardware evidence. Hardware contributors must also
follow the bounded procedure and evidence rules in [docs/HIL.md](docs/HIL.md).

## Commits and pull requests

Use clear, scoped commits. Conventional prefixes such as `feat:`, `fix:`,
`docs:`, `refactor:`, `test:`, and `chore:` are preferred.

A pull request should explain:

- the problem and intended contract;
- the implementation and any compatibility impact;
- validation actually run, including explicit not-run items; and
- documentation/changelog changes.

Breaking API, `Config`, or enum changes require prior discussion and a major
version. New backward-compatible features use a minor version; fixes and
documentation use a patch version when released.

For questions, open a GitHub issue or discussion. Report security concerns
privately as described in [SECURITY.md](SECURITY.md).
