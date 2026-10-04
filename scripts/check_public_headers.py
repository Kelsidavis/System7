#!/usr/bin/env python3
"""Compile public headers independently with strict C and C++ diagnostics."""

import argparse
import os
import shlex
import subprocess
import sys
from pathlib import Path


def compiler_command(value, name):
    command = shlex.split(value)
    if not command:
        raise ValueError(f"{name} must name a compiler")
    return command


def check_headers(root, compiler, language, standard, headers):
    include_dir = root / "include"
    failures = []
    for header in headers:
        include_name = header.relative_to(include_dir).as_posix()
        source = (
            f'#include "{include_name}"\ntypedef int system7_header_check_anchor;\n'
        )
        command = [
            *compiler,
            f"-std={standard}",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-pedantic-errors",
            "-fsyntax-only",
            f"-I{include_dir}",
            "-x",
            language,
            "-",
        ]
        try:
            result = subprocess.run(
                command,
                cwd=root,
                input=source,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )
        except OSError as error:
            raise RuntimeError(
                f"could not run {' '.join(compiler)}: {error}"
            ) from error
        if result.returncode:
            failures.append((header, result.stderr))

    for header, diagnostics in failures:
        print(f"{header.relative_to(root)} ({language}):", file=sys.stderr)
        print(diagnostics, file=sys.stderr, end="")
    return len(failures)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root", type=Path, default=Path(__file__).resolve().parent.parent
    )
    parser.add_argument("--cc", default=os.environ.get("HOST_CC", "cc"))
    parser.add_argument("--cxx", default=os.environ.get("HOST_CXX", "c++"))
    args = parser.parse_args()

    root = args.root.resolve()
    headers = sorted((root / "include").rglob("*.h"))
    if not headers:
        parser.error(f"no public headers found under {root / 'include'}")

    try:
        checks = (
            (compiler_command(args.cc, "--cc"), "c", "gnu11"),
            (compiler_command(args.cxx, "--cxx"), "c++", "c++17"),
        )
        failures = sum(
            check_headers(root, compiler, language, standard, headers)
            for compiler, language, standard in checks
        )
    except (RuntimeError, ValueError) as error:
        print(error, file=sys.stderr)
        return 1

    if failures:
        print(f"{failures} public-header checks failed", file=sys.stderr)
        return 1

    print(
        f"Checked {len(headers)} public headers as standalone C and C++ translation units"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
