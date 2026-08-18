#!/usr/bin/env bash
# Publish the pinned Python version as SPP_PYTHON_VERSION.
#
# Deliberately awk and not pins.py: this runs before Python
# is installed, so the one value needed to install it cannot
# itself be read by Python. Everything else goes through
# pins.py once the interpreter exists.
set -euo pipefail

MANIFEST=".github/dependencies.toml"

version="$(awk -F'"' '
  /^\[pin\.python\]/ { inside = 1; next }
  inside && /^version[[:space:]]*=/ { print $2; exit }
  inside && /^\[/ { exit }
' "$MANIFEST")"

if [ -z "$version" ]; then
  echo "::error::no version under [pin.python] in ${MANIFEST}"
  exit 1
fi

echo "SPP_PYTHON_VERSION=${version}" >> "${GITHUB_ENV:-/dev/stdout}"
