#!/usr/bin/env bash
# Decide whether the restored SPP_LOCAL_PREFIX holds every small
# library. The stamp is written last and holds the records it was
# built from, so a half-finished install or one that predates a
# manifest edit reads as incomplete rather than being trusted.
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
