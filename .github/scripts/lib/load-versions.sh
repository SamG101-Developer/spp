#!/usr/bin/env bash
# Publish every pin in .github/dependencies.toml into the
# job environment, install prefixes included.
set -euo pipefail

PINS=".github/scripts/lib/pins.py"

# Outside CI there is no GITHUB_ENV to append to, so print
# instead: the same output, useful for checking what a
# change to the manifest would export.
if [ -n "${GITHUB_ENV:-}" ]; then
  python3 "$PINS" env >> "$GITHUB_ENV"
else
  python3 "$PINS" env
fi
