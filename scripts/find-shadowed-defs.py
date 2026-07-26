#!/usr/bin/env python3
"""
find-shadowed-defs.py - find code you can edit with no effect on the build.

Two categories, both of which have already cost debugging sessions:

  DEAD FILES      .c files no Makefile configuration ever compiles.
                  QuickDraw/Text.c defines DrawText and is never built; the
                  DrawText that runs is in FontManager/FontManagerCore.c.

  UNBUILT COPIES  A function defined in a compiled .c whose definition does not
                  survive into that .o - excluded by `static`, `#if 0`, or a
                  feature-flag `#ifdef` - while another file supplies the symbol
                  that links.

An unbuilt copy is usually INTENTIONAL: the tree uses mutually exclusive
alternates chosen by feature flags (GetNextEvent is `#ifndef ENABLE_PROCESS_COOP`
in EventManager/event_manager.c, and config/default.mk turns that flag on, so
ProcessMgr/EventIntegration.c wins). The danger is not the mechanism, it is
being pointed at the wrong file: event_manager.c calls its copy the "Canonical
implementation" and EventIntegration.c's comment agrees, yet the canonical one
is compiled out by default. The update-event fix in 293388f was written there
and never ran.

So this script flags an unbuilt copy as SUSPECT when the dead text advertises
itself as canonical/primary/real - that combination is what misleads.

Usage:
    make && python3 scripts/find-shadowed-defs.py
"""

import collections
import os
import re
import subprocess
import sys

FUNC = re.compile(
    r'^([A-Za-z_][A-Za-z0-9_\s\*]*?\**\s*)([A-Za-z_][A-Za-z0-9_]*)\s*\([^;]*\)\s*\{',
    re.M)
KEYWORDS = {'if', 'for', 'while', 'switch', 'return', 'sizeof', 'defined'}
CANONICAL = re.compile(r'canonical|primary implementation|the real (one|impl)',
                       re.I)


def strip_if_zero(text):
    """Drop `#if 0 ... #endif` bodies, tracking nested conditionals."""
    out, skip = [], 0
    for line in text.splitlines(keepends=True):
        s = line.lstrip()
        if skip:
            if re.match(r'#\s*if', s):
                skip += 1
            elif re.match(r'#\s*endif', s):
                skip -= 1
            continue
        if re.match(r'#\s*if\s+0(\s|$)', s):
            skip = 1
            continue
        out.append(line)
    return ''.join(out)


def obj_for(src):
    return 'build/obj/' + src[4:-2] + '.o'


def defined_symbols(obj):
    if not os.path.exists(obj):
        return set()
    out = subprocess.run(['nm', '--defined-only', obj],
                         capture_output=True, text=True).stdout
    return {p[2] for p in (l.split() for l in out.splitlines())
            if len(p) == 3 and p[1] == 'T'}


def main():
    if not os.path.isdir('build/obj'):
        sys.exit('no build/obj - run make first')

    makefile = open('Makefile').read()
    for extra in ('config/default.mk', 'config/release.mk', 'config/debug.mk'):
        if os.path.exists(extra):
            makefile += open(extra).read()

    sources, dead_files = [], []
    for root, _dirs, files in os.walk('src'):
        if 'deprecated' in root:
            continue
        for name in sorted(files):
            if not name.endswith('.c'):
                continue
            path = os.path.join(root, name)
            (sources if path in makefile else dead_files).append(path)

    # symbol -> objects providing it
    sym2obj = collections.defaultdict(list)
    for src in sources:
        for sym in defined_symbols(obj_for(src)):
            sym2obj[sym].append(src)

    print('=== DEAD FILES (never compiled by any configuration) ===')
    hits = 0
    for path in dead_files:
        text = strip_if_zero(open(path, errors='ignore').read())
        names = {m.group(2) for m in FUNC.finditer(text)
                 if 'static' not in m.group(1).split()
                 and m.group(2) not in KEYWORDS}
        shadowing = sorted(n for n in names if n in sym2obj)
        if shadowing:
            hits += 1
            print(f'{path}\n    defines, but the live copy is elsewhere: '
                  f'{", ".join(shadowing[:8])}'
                  f'{" ..." if len(shadowing) > 8 else ""}')
    if not hits:
        print('  (none)')

    print('\n=== UNBUILT COPIES (in a compiled file, excluded from its .o) ===')
    print('Usually an intentional feature-flag alternate. SUSPECT marks a copy'
          '\nwhose own text calls itself canonical - that is what misleads.\n')
    suspect = plain = 0
    for src in sources:
        text = strip_if_zero(open(src, errors='ignore').read())
        built = defined_symbols(obj_for(src))
        for m in FUNC.finditer(text):
            if 'static' in m.group(1).split():
                continue
            fn = m.group(2)
            if fn in KEYWORDS or fn in built or fn not in sym2obj:
                continue
            near = text[max(0, m.start() - 400):m.start()]
            flag = CANONICAL.search(near)
            if flag:
                suspect += 1
                print(f'SUSPECT {fn}\n    dead copy : {src} '
                      f'(claims "{flag.group(0)}")\n'
                      f'    links from: {", ".join(sym2obj[fn])}')
            else:
                plain += 1

    print(f'\nsuspect: {suspect}    other unbuilt copies: {plain}'
          f'    dead files: {hits}')
    return 1 if (suspect or hits) else 0


if __name__ == '__main__':
    sys.exit(main())
