#!/usr/bin/env python3

"""Sort C++ module import declarations.

Handles files of the form:

    module;
    #include <...>
    #include <...> <- optional global-module fragment (sorted)

    [export] module foo.bar;
    import ...;
    import ...; <- sorted: spp.* first (ASCII), then the rest (ASCII)

Rules:
  * Global-module-fragment #includes (between `module;` and the module
    declaration) are sorted case-sensitive ASCII.
  * import declarations immediately following the module declaration are
    sorted case-sensitive ASCII, with `spp.*` imports first and everything
    else (std, llvm, ...) after, each group internally ASCII-sorted.
  * Blank lines / comments inside a run are not reordered across; only
    contiguous runs of imports are sorted.

Auto-fixes in place. Exit code 1 if any file was modified.
"""

from __future__ import annotations
import re
import sys
from typing import Callable

INCLUDE_RE = re.compile(r"^\s*#\s*include\b")
IMPORT_RE = re.compile(r"^\s*(?:export\s+)?import\b")
MODULE_DECL_RE = re.compile(r"^\s*(?:export\s+)?module\b")
MODULE_FRAGMENT_RE = re.compile(r"^\s*module\s*;\s*$")


def import_key(line: str) -> tuple:
    """Sort imports hierarchically, like a directory tree."""
    stripped = line.strip().rstrip(';').strip()
    name = re.sub(r'^(?:export\s+)?import\s+', '', stripped)
    parts = name.split('.')
    branch = parts[1] if len(parts) > 1 else ''
    tail = tuple(parts[2:]) if len(parts) > 2 else ()
    group = 0 if name.startswith("spp.") else 1
    return group, branch, len(parts), tail, name


def include_key(line: str) -> tuple:
    """Sort includes hierarchically, like a directory tree."""
    stripped = line.strip()
    match = re.search(r'include\s*[<"]([^>"]+)[>"]', stripped)
    name = match.group(1) if match else stripped
    parts = name.split('/')
    branch = parts[0] if parts else ''
    tail = tuple(parts[1:]) if len(parts) > 1 else ()
    return branch, len(parts), tail, name


def sort_run(lines: list[str], key: Callable[[str], tuple]) -> list[str]:
    return sorted(lines, key=key)


def process(text: str) -> str:
    lines = text.split("\n")
    out: list[str] = []
    i = 0
    n = len(lines)

    while i < n:
        line = lines[i]

        # Handle contiguous #include directives
        if INCLUDE_RE.match(line):
            j = i
            while j < n and INCLUDE_RE.match(lines[j]):
                j += 1
            run = lines[i:j]
            out.extend(sort_run(run, include_key))
            i = j
            continue

        # Handle contiguous import statements
        if IMPORT_RE.match(line):
            j = i
            while j < n and IMPORT_RE.match(lines[j]):
                j += 1
            run = lines[i:j]
            out.extend(sort_run(run, import_key))
            i = j
            continue

        out.append(line)
        i += 1

    return "\n".join(out)


def main(argv: list[str]) -> int:
    changed = False
    for path in argv[1:]:
        with open(path, 'r', encoding='utf-8') as f:
            original = f.read()
        fixed = process(original)
        if fixed != original:
            with open(path, 'w', encoding='utf-8') as f:
                f.write(fixed)
            print(f'sorted imports: {path}')
            changed = True
    return 1 if changed else 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
