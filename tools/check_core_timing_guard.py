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
    "delay": re.compile(r"\bdelay\s*\("),
    "delayMicroseconds": re.compile(r"\bdelayMicroseconds\s*\("),
    "yield": re.compile(r"\byield\s*\("),
}

INCLUDE_ARDUINO_RE = re.compile(r'^\s*#\s*include\s*[<\"]Arduino\.h[>\"]', re.MULTILINE)
RAW_STRING_START_RE = re.compile(
    r'(?:u8|u|U|L)?R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\('
)
QUOTED_LITERAL_START_RE = re.compile(r'''(?:u8|u|U|L)?(?P<quote>["'])''')
NUMBER_START_RE = re.compile(r"(?:[0-9]|\.[0-9])(?:[eEpP][+-]|[\w.]|'(?=\w))*")


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
    """Scan C++ lexical text; this does not expand macros or evaluate directives."""
    # Phase-2 line splicing joins identifiers and comment delimiters. Remember
    # the joins: a raw-string terminator may not be manufactured by a splice,
    # because C++ preserves physical backslash/newline pairs inside raw bodies.
    parts = re.split(r"\\\r?\n", text)
    splice_positions: set[int] = set()
    length = 0
    for part in parts[:-1]:
        length += len(part)
        splice_positions.add(length)
    text = "".join(parts)
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
            while end >= 0 and any(
                end < position < end + len(terminator)
                for position in splice_positions
            ):
                end = text.find(terminator, end + 1)
            end = len(text) if end < 0 else end + len(terminator)
            output.append(_blank_non_code(text[cursor:end]))
            cursor = end
            continue

        # Consume preprocessing numbers before recognizing apostrophes. A
        # character literal can directly follow a keyword (return'(';), so
        # the previous character alone cannot distinguish it from a separator.
        number = NUMBER_START_RE.match(text, cursor) if at_token_start else None
        if number is not None:
            output.append(number.group())
            cursor = number.end()
            continue

        literal = QUOTED_LITERAL_START_RE.match(text, cursor)
        if literal is not None:
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

    adjacent = strip_non_code(
        "char open() { return'('; }\n"
        "void adjacent() { millis(); auto close = ')'; }\n"
    )
    if len(FORBIDDEN_CALLS["millis"].findall(adjacent)) != 1:
        raise RuntimeError("strip_non_code keyword-adjacent character self-test failed")

    for number in ("400'000", "1'000'000", "0xAB'CD", "0b10'01", "1.25",
                   ".5", "1e+1'0", "0x1.fp+1'0"):
        code = strip_non_code(f"auto value = {number}; millis(); auto close = ')';")
        if len(FORBIDDEN_CALLS["millis"].findall(code)) != 1:
            raise RuntimeError(f"strip_non_code number {number!r} self-test failed")

    for name, pattern in FORBIDDEN_CALLS.items():
        spliced = strip_non_code(name[:2] + "\\\n" + name[2:] + "();")
        if len(pattern.findall(spliced)) != 1:
            raise RuntimeError(f"strip_non_code spliced {name} self-test failed")
        hidden = strip_non_code(
            f'// continued comment \\\n{name}();\n'
            f'/\\\n* {name}(); */\n'
            f'auto raw = R"tag()ta\\\ng"; {name}();)tag";\n'
            f'auto text = "{name}()";\n'
        )
        if pattern.search(hidden) is not None:
            raise RuntimeError(f"strip_non_code hidden {name} self-test failed")


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
