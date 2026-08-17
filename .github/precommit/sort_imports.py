#!/usr/bin/env python3

"""Sort C++ module import declarations.

Handles files of the form:

    module;
    #include <...> <- optional global-module fragment (sorted)
    #include <...>

    [export] module foo.bar;
    import ...; <- sorted: spp.* first (ASCII), then the rest (ASCII)
    import ...;

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

INCLUDE_RE = re.compile(r"^\s*#\s*include\b")
IMPORT_RE = re.compile(r"^\s*(?:export\s+)?import\b")
MODULE_DECL_RE = re.compile(r"^\s*(?:export\s+)?module\b")
MODULE_FRAGMENT_RE = re.compile(r"^\s*module\s*;\s*$")


def import_key(line: str) -> tuple[int, str]:
    stripped = line.strip().rstrip(";").strip()
    name = re.sub(r'^(?:export\s+)?import\s+', '', stripped)
    group = 0 if name.startswith("spp.") else 1
    return group, name


def sort_run(lines: list[str], key: Callable[[str], tuple[int, str]]) -> list[str]:
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
            out.extend(sorted(run, key=lambda s: s.strip()))
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
