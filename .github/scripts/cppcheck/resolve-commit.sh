#!/usr/bin/env bash
# Resolve the requested cppcheck ref to a commit, so the cache key pins the exact build rather than a moving branch.
set -euo pipefail

# Resolve the version from the hash being passed in under the "CPPCHECK_REF" flag. If the hash cannot be resolved then
# this script has failed and the cppcheck step cannot continue.
sha=$(git ls-remote https://github.com/danmar/cppcheck.git "$CPPCHECK_REF" | cut -f1)
if [ -z "$sha" ]; then
  echo "::error::cppcheck ref '$CPPCHECK_REF' does not exist"
  exit 1
fi

# Slugify the ref so it can sit inside the cache key: a raw ref may carry slashes, and the key's shape matters because
# the nightly pruner groups on everything ahead of the final segment.
slug="$(printf '%s' "$CPPCHECK_REF" | tr -c 'A-Za-z0-9._' '-' | sed 's/-\{2,\}/-/g; s/^-//; s/-$//')"

# Log the hash and the slug to the console.
{
  echo "sha=$sha"
  echo "ref-slug=$slug"
} >> "$GITHUB_OUTPUT"
