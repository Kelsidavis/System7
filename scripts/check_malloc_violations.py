#!/usr/bin/env python3
"""Reject direct C-library allocation calls in kernel sources."""

import argparse
import re
import sys
from pathlib import Path


ALLOCATOR_CALL = re.compile(r"\b(malloc|calloc|realloc|free)\s*\(")
INTERNAL_ALLOCATOR = Path("src/MemoryMgr/MemoryManager.c")


def mask_comments_and_literals(source):
    """Replace comments and quoted literals with spaces, preserving newlines."""
    result = list(source)
    state = "code"
    index = 0

    while index < len(source):
        char = source[index]
        next_char = source[index + 1] if index + 1 < len(source) else ""

        if state == "code":
            if char == "/" and next_char == "/":
                result[index] = result[index + 1] = " "
                state = "line-comment"
                index += 2
                continue
            if char == "/" and next_char == "*":
                result[index] = result[index + 1] = " "
                state = "block-comment"
                index += 2
                continue
            if char == '"':
                result[index] = " "
                state = "string"
            elif char == "'":
                result[index] = " "
                state = "character"

        elif state == "line-comment":
            if char == "\n":
                previous = source[index - 1] if index else ""
                if previous == "\r" and index > 1:
                    previous = source[index - 2]
                if previous != "\\":
                    state = "code"
            else:
                result[index] = " "

        elif state == "block-comment":
            if char == "*" and next_char == "/":
                result[index] = result[index + 1] = " "
                state = "code"
                index += 2
                continue
            if char != "\n":
                result[index] = " "

        else:  # string or character literal
            if char == "\\" and index + 1 < len(source):
                if char != "\n":
                    result[index] = " "
                if source[index + 1] != "\n":
                    result[index + 1] = " "
                index += 2
                continue
            if char == ('"' if state == "string" else "'"):
                state = "code"
            if char != "\n":
                result[index] = " "

        index += 1

    return "".join(result)


def main():
    default_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root", type=Path, default=default_root,
        help="repository root (default: detected from this script)",
    )
    args = parser.parse_args()
    source_root = args.root / "src"
    if not source_root.is_dir():
        parser.error(f"source directory not found: {source_root}")

    violations = []
    checked = 0
    for path in sorted(source_root.rglob("*.c")):
        relative = path.relative_to(args.root)
        if relative == INTERNAL_ALLOCATOR:
            continue
        checked += 1
        source = mask_comments_and_literals(path.read_text(errors="replace"))
        for match in ALLOCATOR_CALL.finditer(source):
            line = source.count("\n", 0, match.start()) + 1
            violations.append((relative, line, match.group(1)))

    if violations:
        for path, line, function in violations:
            print(
                f"{path}:{line}: direct {function}() call; "
                "use the Memory Manager API",
                file=sys.stderr,
            )
        print(
            f"Found {len(violations)} allocation violation(s) in {checked} C files.",
            file=sys.stderr,
        )
        return 1

    print(f"No direct C allocator calls found in {checked} C source files.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
