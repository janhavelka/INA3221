#!/usr/bin/env python3
"""Compile the framework-neutral core with strict GCC/Clang diagnostics."""

from __future__ import annotations

import os
import pathlib
import shutil
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]


def find_compiler() -> str | None:
    configured = os.environ.get("CXX")
    if configured:
        return configured
    for candidate in ("c++", "g++", "clang++"):
        resolved = shutil.which(candidate)
        if resolved:
            return resolved
    return None


def main() -> int:
    compiler = find_compiler()
    if compiler is None:
        print("Strict host compile FAILED: no C++ compiler found", file=sys.stderr)
        return 1

    command = [
        compiler,
        "-std=c++17",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
        "-Wconversion",
        "-Wsign-conversion",
        f"-I{ROOT / 'include'}",
        "-fsyntax-only",
        str(ROOT / "src" / "INA3221.cpp"),
    ]
    completed = subprocess.run(command, cwd=ROOT, check=False)
    if completed.returncode != 0:
        print(
            f"Strict host compile FAILED with {pathlib.Path(compiler).name}",
            file=sys.stderr,
        )
        return completed.returncode

    print(f"Strict host compile PASSED with {pathlib.Path(compiler).name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
