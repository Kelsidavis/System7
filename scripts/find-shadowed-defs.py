#!/usr/bin/env python3
"""
find-shadowed-defs.py - report functions defined in a compiled source file whose
definition is NOT the one that links.

Editing a shadowed definition has no effect on the running system. This has
already cost two debugging sessions: the update-event synthesis was written into
EventManager/event_manager.c's GetNextEvent (dead - ProcessMgr/EventIntegration.c
wins), and QuickDraw/Text.c's DrawText is dead (FontManager/FontManagerCore.c
wins).

Run after a build:
    make && python3 scripts/find-shadowed-defs.py

Cross-platform alternates (Platform/arm, arm64, ppc when building x86) are
expected and are listed separately from same-platform shadowing, which is the
category worth acting on.
"""

import collections
import os
import re
import subprocess
import sys

FUNC = re.compile(
    r'^[A-Za-z_][A-Za-z0-9_\s\*]*?\**\s*([A-Za-z_][A-Za-z0-9_]*)\s*\([^;]*\)\s*\{',
    re.M)
KEYWORDS = {'if', 'for', 'while', 'switch', 'return', 'sizeof', 'defined'}
OTHER_PLATFORMS = ('src/Platform/arm/', 'src/Platform/arm64/', 'src/Platform/ppc/')


def source_definitions():
    defs = collections.defaultdict(set)
    for root, _dirs, files in os.walk('src'):
        if 'deprecated' in root:
            continue
        for name in files:
            if not name.endswith('.c'):
                continue
            path = os.path.join(root, name)
            with open(path, errors='ignore') as fh:
                text = fh.read()
            for match in FUNC.finditer(text):
                fn = match.group(1)
                if fn not in KEYWORDS:
                    defs[fn].add(path)
    return defs


def object_symbols():
    sym2obj = collections.defaultdict(list)
    for root, _dirs, files in os.walk('build/obj'):
        for name in files:
            if not name.endswith('.o'):
                continue
            path = os.path.join(root, name)
            out = subprocess.run(['nm', '--defined-only', path],
                                 capture_output=True, text=True).stdout
            for line in out.splitlines():
                parts = line.split()
                if len(parts) == 3 and parts[1] == 'T':
                    sym2obj[parts[2]].append(path)
    return sym2obj


def main():
    if not os.path.isdir('build/obj'):
        sys.exit('no build/obj - run make first')

    makefile = open('Makefile').read()
    defs = source_definitions()
    sym2obj = object_symbols()

    same, cross = [], []
    for fn, srcs in sorted(defs.items()):
        if len(srcs) < 2 or fn not in sym2obj:
            continue
        winners = sym2obj[fn]
        compiled = [s for s in srcs if s in makefile]
        shadowed = [s for s in compiled
                    if 'build/obj/' + s[4:-2] + '.o' not in winners]
        if not shadowed:
            continue
        entry = (fn, winners, shadowed)
        if all(s.startswith(OTHER_PLATFORMS) for s in shadowed):
            cross.append(entry)
        else:
            same.append(entry)

    print('=== SAME-PLATFORM SHADOWING (act on these) ===')
    print('The dead copy is compiled for this platform but loses at link time.\n')
    for fn, winners, shadowed in same:
        print(f'{fn}\n    links from: {", ".join(winners)}\n'
              f'    dead copy : {", ".join(shadowed)}')
    print(f'\nsame-platform: {len(same)}    cross-platform alternates: {len(cross)}')
    return 1 if same else 0


if __name__ == '__main__':
    sys.exit(main())
