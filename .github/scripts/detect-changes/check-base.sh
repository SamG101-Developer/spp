#!/usr/bin/env bash
# Report whether the base commit is one we can diff against.
# changed-files derives no verdict without a reachable base,
# and its "no verdict" reads as "nothing changed", which is
# the one direction this gate must never fail in.
set -euo pipefail

if [ -z "$BASE_SHA" ] \
  || [ "$BASE_SHA" = "0000000000000000000000000000000000000000" ] \
  || ! git cat-file -e "${BASE_SHA}^{commit}" 2>/dev/null; then
  echo "no usable base commit; assuming everything changed"
  echo "usable=false" >> "$GITHUB_OUTPUT"
else
  echo "usable=true" >> "$GITHUB_OUTPUT"
fi
