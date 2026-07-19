#!/usr/bin/env python3
"""Verify that release-version metadata agrees with library.json."""

from __future__ import annotations

import json
import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
SEMVER_RE = re.compile(r"^\d+\.\d+\.\d+$")


def read_text(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8", errors="strict")


def captured_version(relative_path: str, pattern: str, label: str) -> str:
    matches = re.findall(pattern, read_text(relative_path), flags=re.MULTILINE)
    if not matches:
        raise ValueError(f"{label}: version field not found in {relative_path}")
    if len(matches) != 1:
        raise ValueError(
            f"{label}: expected one version field in {relative_path}, found {len(matches)}"
        )
    return matches[0]


def main() -> int:
    errors: list[str] = []

    try:
        library_data = json.loads(read_text("library.json"))
        expected = str(library_data["version"])
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print(f"Metadata consistency FAILED: cannot read library.json version: {exc}")
        return 1

    if SEMVER_RE.fullmatch(expected) is None:
        errors.append(f"library.json: invalid semantic version {expected!r}")

    checks = (
        (
            "idf_component.yml",
            r'^version:\s*["\']?(\d+\.\d+\.\d+)["\']?\s*$',
            "ESP-IDF component version",
        ),
        (
            "include/INA3221/Version.h",
            r'^#define\s+INA3221_VERSION_STRING\s+"(\d+\.\d+\.\d+)"\s*$',
            "generated public-header version",
        ),
        (
            "Doxyfile",
            r'^PROJECT_NUMBER\s*=\s*["\']?(\d+\.\d+\.\d+)["\']?\s*$',
            "Doxygen project version",
        ),
        (
            "README.md",
            r'^Library version:\s*`v(\d+\.\d+\.\d+)`\s*$',
            "README documented version",
        ),
    )

    for relative_path, pattern, label in checks:
        try:
            observed = captured_version(relative_path, pattern, label)
        except (OSError, UnicodeError, ValueError) as exc:
            errors.append(str(exc))
            continue
        if observed != expected:
            errors.append(
                f"{label}: {relative_path} has {observed}, expected {expected}"
            )

    if errors:
        print("Metadata consistency FAILED:")
        for error in errors:
            print(f"- {error}")
        return 1

    print(f"Metadata consistency PASSED: {expected}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
