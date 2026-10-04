#!/usr/bin/env python3
"""Check that relative inline links in Markdown files point to existing paths."""

import argparse
import re
from pathlib import Path
from urllib.parse import unquote, urlsplit


LINK = re.compile(r"\]\(\s*(?:<([^>]+)>|([^\s)]+))")
FENCED_BLOCK = re.compile(r"(?ms)^\s*(```|~~~).*?^\s*\1\s*$")
SKIP_DIRS = {".git", "build", "node_modules", ".venv"}


def markdown_files(root):
    return sorted(
        path
        for path in root.rglob("*.md")
        if not any(part in SKIP_DIRS for part in path.relative_to(root).parts)
    )


def broken_links(root):
    for document in markdown_files(root):
        text = FENCED_BLOCK.sub("", document.read_text(errors="replace"))
        for match in LINK.finditer(text):
            target = match.group(1) or match.group(2)
            parsed = urlsplit(target)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            destination = (document.parent / unquote(parsed.path)).resolve()
            if not destination.exists():
                yield document, target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="repository root to check (default: detected from this script)",
    )
    root = parser.parse_args().root.resolve()
    failures = list(broken_links(root))
    if failures:
        for document, target in failures:
            print(f"{document.relative_to(root)}: broken relative link: {target}")
        return 1
    print(f"Checked relative Markdown links under {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
