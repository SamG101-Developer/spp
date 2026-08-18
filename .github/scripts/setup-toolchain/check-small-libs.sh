#!/usr/bin/env bash
# Decide whether the restored SPP_LOCAL_PREFIX already holds
# every small library. install-small-libs.sh writes the stamp
# as its last act, so a prefix cached from an install that
# died halfway reads as incomplete and gets rebuilt rather
# than trusted. The stamp holds the library records it was
# built from, so a tree that predates a manifest edit is
# rejected too.
set -euo pipefail

PREFIX="$SPP_LOCAL_PREFIX"
if [ "$RUNNER_OS" = "Windows" ]; then
  PREFIX="$(cygpath -m "$PREFIX")"
fi

complete=false
if [ -f "$PREFIX/.spp-libs-stamp" ] &&
  python3 .github/scripts/lib/pins.py libraries | diff -q - "$PREFIX/.spp-libs-stamp" >/dev/null; then
  complete=true
fi

echo "small libraries complete: $complete"
echo "complete=$complete" >> "$GITHUB_OUTPUT"
