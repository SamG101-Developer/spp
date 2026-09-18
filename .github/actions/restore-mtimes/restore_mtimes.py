#!/usr/bin/env python3
"""Set every tracked file's mtime to the time of the last commit that touched it.

`actions/checkout` writes the whole tree at once, so every file lands with the same brand-new mtime, and Ninja - which
compares source mtimes against object mtimes - sees a restored build tree as entirely stale. Dating each file by its
last commit makes an unchanged file older than the cached objects and a changed one newer, so Ninja rebuilds exactly
what moved. That is what makes CI incremental.

Equivalent to MestreLion/git-tools' `git-restore-mtime`, vendored to keep a network dependency out of every job.
"""

from __future__ import annotations

import os
import subprocess
import sys


def git(*args: str) -> str:
    return subprocess.run(["git", *args], capture_output=True, text=True, check=True).stdout


def main() -> int:
    os.chdir(git("rev-parse", "--show-toplevel").strip())

    # Paths still awaiting an mtime. Walking history newest-first means the first
    # commit that mentions a path is the last one to have touched it.
    pending = set(git("ls-files").splitlines())
    if not pending:
        print("no tracked files; nothing to do")
        return 0

    total = len(pending)
    stamped = 0
    timestamp: int | None = None

    # `--no-renames` keeps paths literal (no `{old => new}` rewriting) and
    # `core.quotepath=off` keeps non-ASCII paths byte-identical to `ls-files`.
    log = subprocess.Popen(
        [
            "git",
            "-c",
            "core.quotepath=off",
            "log",
            "--format=@%at",
            "--name-only",
            "--no-renames",
            "HEAD",
        ],
        stdout=subprocess.PIPE,
        text=True,
    )
    assert log.stdout is not None

    for line in log.stdout:
        line = line.rstrip("\n")
        if not line:
            continue
        if line.startswith("@"):
            timestamp = int(line[1:])
        elif timestamp is not None and line in pending:
            pending.discard(line)
            try:
                os.utime(line, (timestamp, timestamp))
                stamped += 1
            except OSError as exc:  # symlink to nowhere, permissions, ...
                print(f"warning: could not stamp {line}: {exc}", file=sys.stderr)
            if not pending:
                break

    # Draining the pipe stops git dying of SIGPIPE once we break out early.
    log.stdout.close()
    log.wait()

    print(f"restored mtimes on {stamped}/{total} tracked files")
    if pending:
        # Shallow clones cannot reach the commit that introduced these paths.
        print(f"{len(pending)} file(s) not found in the fetched history", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
