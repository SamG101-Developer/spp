#!/usr/bin/env python3
"""Pack one platform's release zip: the executable, then the files that ship beside it, all at the zip's root.

    pack.py <out.zip> <executable> [<file> ...]

Each entry is named by its file name alone, so the executable unpacks as plain "spp"/"spp.exe". Entries record Unix
permissions (0755 for the executable, 0644 for the rest), which unzip and bsdtar restore, so an unpacked "spp" runs
without a chmod. Every entry has the same fixed time, so the bytes depend only on the files, not on when this ran.
"""

from __future__ import annotations

import sys
import zipfile
from pathlib import Path

# The earliest time a zip entry can hold.
FIXED_TIME = (1980, 1, 1, 0, 0, 0)
UNIX = 3


def entry(path: Path, mode: int) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(path.name, date_time=FIXED_TIME)
    info.create_system = UNIX
    info.external_attr = (0o100000 | mode) << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    return info


def main(argv: list[str]) -> int:
    if len(argv) < 3:
        print("::error::usage: pack.py <out.zip> <executable> [<file> ...]", file=sys.stderr)
        return 1
    out = Path(argv[1])
    files = [Path(arg) for arg in argv[2:]]

    if out.exists():
        print(f"::error::{out} already exists", file=sys.stderr)
        return 1
    names = [path.name for path in files]
    if len(set(names)) != len(names):
        print(f"::error::two files share a name: {names}", file=sys.stderr)
        return 1
    for path in files:
        if not path.is_file():
            print(f"::error::{path} is not a file", file=sys.stderr)
            return 1

    with zipfile.ZipFile(out, "w") as archive:
        for index, path in enumerate(files):
            archive.writestr(entry(path, 0o755 if index == 0 else 0o644), path.read_bytes(), compresslevel=9)

    print(f"packed {out.name}: {', '.join(names)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
