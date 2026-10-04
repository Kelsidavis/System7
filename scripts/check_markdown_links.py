#!/usr/bin/env python3
"""Check Markdown links and inline repository paths for missing targets."""

import argparse
import re
from pathlib import Path
from urllib.parse import unquote, urlsplit


LINK = re.compile(r"\]\(\s*(?:<([^>]+)>|([^\s)]+))")
REFERENCE_LINK = re.compile(r"(?m)^\s{0,3}\[[^]\n]+\]:\s*(?:<([^>]+)>|([^\s]+))")
REPO_PATH = re.compile(r"`((?:src|include|tests|scripts|tools|docs)/[A-Za-z0-9_./-]+)`")
FENCED_BLOCK = re.compile(r"(?ms)^\s*(```|~~~).*?^\s*\1\s*$")
SKIP_DIRS = {".git", "build", "node_modules", ".venv"}


def markdown_files(root):
    return sorted(
        path
        for path in root.rglob("*.md")
        if not any(part in SKIP_DIRS for part in path.relative_to(root).parts)
    )


def is_broken_relative_link(document, target):
    parsed = urlsplit(target)
    if parsed.scheme or parsed.netloc or not parsed.path:
        return False
    destination = (document.parent / unquote(parsed.path)).resolve()
    return not destination.exists()


def broken_links(root):
    for document in markdown_files(root):
        text = FENCED_BLOCK.sub("", document.read_text(errors="replace"))
        for pattern in (LINK, REFERENCE_LINK):
            for match in pattern.finditer(text):
                target = match.group(1) or match.group(2)
                if is_broken_relative_link(document, target):
                    yield document, target


def broken_repo_paths(root):
    for document in markdown_files(root):
        text = FENCED_BLOCK.sub("", document.read_text(errors="replace"))
        for match in REPO_PATH.finditer(text):
            target = match.group(1)
            if not (root / target).exists():
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
    path_failures = list(broken_repo_paths(root))
    if failures:
        for document, target in failures:
            print(f"{document.relative_to(root)}: broken relative link: {target}")
    if path_failures:
        for document, target in path_failures:
            print(f"{document.relative_to(root)}: missing repository path: {target}")
    if failures or path_failures:
        return 1
    print(f"Checked Markdown links and repository paths under {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
