#!/usr/bin/env python3
"""Check Markdown links and inline repository paths for missing targets."""

import argparse
import html
import re
from pathlib import Path
from urllib.parse import unquote, urlsplit


LINK = re.compile(r"\]\(\s*(?:<([^>]+)>|([^\s)]+))")
REFERENCE_LINK = re.compile(r"(?m)^\s{0,3}\[[^]\n]+\]:\s*(?:<([^>]+)>|([^\s]+))")
REPO_PATH = re.compile(r"`((?:src|include|tests|scripts|tools|docs)/[A-Za-z0-9_./-]+)`")
FENCED_BLOCK = re.compile(r"(?ms)^\s*(```|~~~).*?^\s*\1\s*$")
INLINE_CODE_SPAN = re.compile(r"(?<!`)(`+)(?!`).*?(?<!`)\1(?!`)", re.DOTALL)
ATX_HEADING = re.compile(r"^ {0,3}#{1,6}[ \t]+(.+?)\s*#*\s*$")
SETEXT_HEADING = re.compile(r"^ {0,3}(=+|-+)\s*$")
HTML_ANCHOR = re.compile(r"<a\s+(?:id|name)=['\"]([^'\"]+)['\"]", re.IGNORECASE)
SKIP_DIRS = {".git", "build", "node_modules", ".venv"}


def markdown_files(root):
    return sorted(
        path
        for path in root.rglob("*.md")
        if not any(part in SKIP_DIRS for part in path.relative_to(root).parts)
    )


def is_broken_relative_link(document, target):
    parsed = urlsplit(target)
    if parsed.scheme or parsed.netloc:
        return False
    destination = (
        (document.parent / unquote(parsed.path)).resolve() if parsed.path else document
    )
    if not destination.exists():
        return True
    if parsed.fragment and destination.suffix.lower() == ".md":
        return not has_heading_fragment(destination, unquote(parsed.fragment))
    return False


def heading_slug(text):
    text = html.unescape(text)
    text = re.sub(r"!?\[([^]]+)\]\([^)]*\)", r"\1", text)
    text = re.sub(r"`+([^`]+)`+", r"\1", text)
    text = re.sub(r"<[^>]+>", "", text)
    slug = "".join(char for char in text.lower() if char.isalnum() or char in " -_")
    return re.sub(r"\s+", "-", slug).strip("-")


def has_heading_fragment(document, fragment):
    if fragment.lower() in {"top", "top-of-page"}:
        return True
    text = FENCED_BLOCK.sub("", document.read_text(errors="replace"))
    headings = set()
    for match in HTML_ANCHOR.finditer(text):
        headings.add(match.group(1))

    lines = text.splitlines()
    slugs = {}
    for index, line in enumerate(lines):
        match = ATX_HEADING.match(line)
        if match:
            title = match.group(1)
        elif index + 1 < len(lines) and SETEXT_HEADING.match(lines[index + 1]):
            title = line.strip()
        else:
            continue

        slug = heading_slug(title)
        occurrence = slugs.get(slug, 0)
        slugs[slug] = occurrence + 1
        headings.add(slug if occurrence == 0 else f"{slug}-{occurrence}")

    return fragment in headings


def mask_inline_code(text):
    return INLINE_CODE_SPAN.sub(
        lambda match: "".join("\n" if char == "\n" else " " for char in match.group()),
        text,
    )


def broken_links(root):
    for document in markdown_files(root):
        text = FENCED_BLOCK.sub("", document.read_text(errors="replace"))
        link_text = mask_inline_code(text)
        for pattern in (LINK, REFERENCE_LINK):
            for match in pattern.finditer(link_text):
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
