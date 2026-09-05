#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCAN_DIRS = ("src", "include")
VALID_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp"}

FORBIDDEN_CALLS = {
    "millis": re.compile(r"\bmillis\s*\("),
    "micros": re.compile(r"\bmicros\s*\("),
    "delayMicroseconds": re.compile(r"\bdelayMicroseconds\s*\("),
    "yield": re.compile(r"\byield\s*\("),
}

INCLUDE_ARDUINO_RE = re.compile(r'^\s*#\s*include\s*[<\"]Arduino\.h[>\"]', re.MULTILINE)
RAW_STRING_START_RE = re.compile(
    r'(?:u8|u|U|L)?R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\('
)
QUOTED_LITERAL_START_RE = re.compile(r'''(?:u8|u|U|L)?(?P<quote>["'])''')


def _blank_non_code(text: str) -> str:
    return "".join("\n" if char == "\n" else " " for char in text)


def _quoted_literal_end(text: str, start: int, quote: str) -> int:
    cursor = start + 1
    while cursor < len(text):
        if text[cursor] == "\\":
            cursor = min(cursor + 2, len(text))
        elif text[cursor] == quote:
            return cursor + 1
        else:
            cursor += 1
    return len(text)

def strip_non_code(text: str) -> str:
    """Blank comments and C++ literals without letting either spoof the other."""
    output: list[str] = []
    cursor = 0
    while cursor < len(text):
        if text.startswith("//", cursor):
            end = text.find("\n", cursor + 2)
            if end < 0:
                end = len(text)
            output.append(_blank_non_code(text[cursor:end]))
            cursor = end
            continue
        if text.startswith("/*", cursor):
            end = text.find("*/", cursor + 2)
            end = len(text) if end < 0 else end + 2
            output.append(_blank_non_code(text[cursor:end]))
            cursor = end
            continue

        raw = RAW_STRING_START_RE.match(text, cursor)
        at_token_start = cursor == 0 or not (
            text[cursor - 1].isalnum() or text[cursor - 1] == "_"
        )
        if raw is not None and at_token_start:
            terminator = ")" + raw.group("delimiter") + '"'
            end = text.find(terminator, raw.end())
            end = len(text) if end < 0 else end + len(terminator)
            output.append(_blank_non_code(text[cursor:end]))
            cursor = end
            continue

        literal = QUOTED_LITERAL_START_RE.match(text, cursor)
        if literal is not None and (
            at_token_start or literal.group("quote") == '"'
        ):
            end = _quoted_literal_end(text, literal.end() - 1,
                                      literal.group("quote"))
            output.append(_blank_non_code(text[cursor:end]))
            cursor = end
            continue

        output.append(text[cursor])
        cursor += 1
    return "".join(output)


def verify_strip_non_code() -> None:
    adversarial = (
        'const char* line = "// hidden"; millis();\n'
        '/* unmatched quote \" */ micros();\n'
        'const char* raw = R"tag(/* delayMicroseconds() */)tag"; yield();\n'
        '// millis(); "\n'
    )
    code = strip_non_code(adversarial)
    for call_name in ("millis", "micros", "yield"):
        if len(FORBIDDEN_CALLS[call_name].findall(code)) != 1:
            raise RuntimeError(f"strip_non_code self-test failed for {call_name}")
    if FORBIDDEN_CALLS["delayMicroseconds"].search(code) is not None:
        raise RuntimeError("strip_non_code raw-string self-test failed")

    separated = strip_non_code(
        "static const unsigned fast = 400'000;\n"
        "void separated() { millis(); }\n"
        "static const unsigned slow = 100'000;\n"
    )
    if len(FORBIDDEN_CALLS["millis"].findall(separated)) != 1:
        raise RuntimeError("strip_non_code digit-separator self-test failed")

    balanced = strip_non_code(
        "static const unsigned clock = 1'000'000;\n"
        "void balanced() { millis(); }\n"
    )
    if len(FORBIDDEN_CALLS["millis"].findall(balanced)) != 1:
        raise RuntimeError("strip_non_code balanced digit-separator self-test failed")

    for prefix in ("", "L", "u", "U", "u8"):
        literals = strip_non_code(
            f"auto open = {prefix}'('; millis(); auto close = {prefix}')';\n"
            f'auto text = {prefix}"/* micros() */";\n'
        )
        if len(FORBIDDEN_CALLS["millis"].findall(literals)) != 1:
            raise RuntimeError(f"strip_non_code {prefix!r} character-literal self-test failed")
        if FORBIDDEN_CALLS["micros"].search(literals) is not None:
            raise RuntimeError(f"strip_non_code {prefix!r} string-literal self-test failed")


def collect_sources() -> list[pathlib.Path]:
    files: list[pathlib.Path] = []
    for dirname in SCAN_DIRS:
        root = ROOT / dirname
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.is_file() and path.suffix.lower() in VALID_SUFFIXES:
                files.append(path)
    return files


def main() -> int:
    verify_strip_non_code()
    observed_calls: dict[str, dict[str, int]] = {}
    observed_includes: dict[str, int] = {}

    for path in collect_sources():
        rel = path.relative_to(ROOT).as_posix()
        raw = path.read_text(encoding="utf-8", errors="replace")
        code = strip_non_code(raw)

        call_counts: dict[str, int] = {}
        for call_name, pattern in FORBIDDEN_CALLS.items():
            count = len(pattern.findall(code))
            if count > 0:
                call_counts[call_name] = count
        if call_counts:
            observed_calls[rel] = call_counts

        include_count = len(INCLUDE_ARDUINO_RE.findall(raw))
        if include_count > 0:
            observed_includes[rel] = include_count

    errors: list[str] = []

    for rel, counts in observed_calls.items():
        errors.append(f"forbidden timing calls in core file: {rel} -> {counts}")

    for rel, count in observed_includes.items():
        errors.append(f"Arduino include in core file: {rel} -> {count}")

    if errors:
        print("Core timing guard FAILED:")
        for err in errors:
            print(f"- {err}")
        return 1

    print("Core timing guard PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
