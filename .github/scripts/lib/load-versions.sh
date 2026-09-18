#!/usr/bin/env bash
# Publish every pin in .github/dependencies.toml into the
# job environment, install prefixes included.
set -euo pipefail

PINS=".github/scripts/lib/pins.py"

# Outside CI there is no GITHUB_ENV, so print instead: useful for
# checking what a manifest change would export.
if [ -n "${GITHUB_ENV:-}" ]; then
  python3 "$PINS" env >> "$GITHUB_ENV"
else
  python3 "$PINS" env
fi
