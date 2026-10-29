# Contributing

Contributions are welcome when they preserve the library's deterministic,
framework-neutral embedded boundary. Read the repository's `AGENTS.md` before
changing public API or core behavior; it contains the binding engineering
rules.

## Development setup

Required for the full host/documentation pass:

- Python 3 (CI uses 3.11)
- PlatformIO Core (CI pins 6.1.19)
- a C++17 host compiler
- Doxygen

ESP-IDF `6.0.1` is additionally required to reproduce the native example builds.
Hardware-in-the-loop checks require a supported ESP32-S2/S3 fixture and are not
implied by a successful host build.

On Windows, use `.\scripts\pio.cmd`; it selects the existing VS Code-managed
PlatformIO installation. If it is missing, report that rather than installing
another Core.

Fetch and inspect the current branch/upstream before editing; fast-forward only
when safe. Keep an existing promptset on its branch unless the user requests
another branch. Make the smallest coherent change and preserve unrelated
worktree changes. `library.json` is the version source of truth;
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

```powershell
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python tools/check_core_timing_guard.py
python tools/check_metadata_consistency.py
python scripts/generate_version.py check
python tools/check_strict_compile.py
python tools/hil_cli_runner.py --parser-self-test
doxygen Doxyfile
.\scripts\pio.cmd test -e native
.\scripts\pio.cmd run -e esp32s3dev
.\scripts\pio.cmd run -e esp32s2dev
.\scripts\pio.cmd pkg pack
git diff --check
```

When ESP-IDF is installed, also run both native targets:

```bash
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

Report unavailable toolchains as `NOT RUN`; do not treat a static contract check
as compiler, linker, or hardware evidence. Hardware contributors must also
follow the bounded procedure and evidence rules in [docs/HIL.md](docs/HIL.md).

Documentation-only changes normally need the contract, version/metadata, local
link and Doxygen checks above. Use native/strict checks for behavior changes.
Major behavior, integration, toolchain and release changes require the exact
final commit's terminal CI results; follow `AGENTS.md` for scope and triggers.
CI runs on pushes/PRs to `main`; it has no manual dispatch trigger.

## Commits and pull requests

Stage only owned files or hunks and use clear, scoped commits. Conventional
prefixes such as `feat:`, `fix:`,
`docs:`, `refactor:`, `test:`, and `chore:` are preferred.

A pull request should explain:

- the problem and intended contract;
- the implementation and any compatibility impact;
- validation actually run, including explicit not-run items; and
- documentation/changelog changes.

Breaking API, `Config`, or enum changes require prior discussion and a major
version. New backward-compatible features use a minor version; fixes and
documentation use a patch version when released.

## Release procedure

Prepare releases from an up-to-date `main` branch with no unrelated changes.
`library.json` is the version source of truth; use the generator rather than
editing `Version.h`:

```powershell
python scripts/generate_version.py set X.Y.Z
```

Update `idf_component.yml`, the Doxygen project version and brief, the README
version/install tag, and promote the changelog's `[Unreleased]` entries to a
dated release section. Run the complete validation matrix above, package the
library, and review any applicable HIL evidence. Then commit and push the
release preparation:

```powershell
git add -- library.json idf_component.yml Doxyfile README.md CHANGELOG.md include/INA3221/Version.h
git commit -m "Release vX.Y.Z"
git push origin main
$releaseCommit = git rev-parse HEAD
gh run list --workflow ci.yml --branch main --commit $releaseCommit --json databaseId,headSha,status,conclusion
```

Select the CI run whose `headSha` matches `$releaseCommit`, then replace
`RUN_ID` below with its `databaseId`:

```powershell
gh run watch RUN_ID --exit-status
gh run view RUN_ID --json headSha,status,conclusion
```

Create the annotated tag only after that exact commit's run has completed
successfully and HEAD still equals `$releaseCommit`:

```powershell
git tag -a vX.Y.Z -m "Release vX.Y.Z"
git push origin refs/tags/vX.Y.Z
```

On GitHub, create a new release from the existing tag, use
`INA3221 vX.Y.Z` as the title, copy/review the matching changelog section, and
publish it. Do not move or reuse a published release tag.

For questions, open a GitHub issue or discussion. Report security concerns
privately as described in [SECURITY.md](SECURITY.md).
