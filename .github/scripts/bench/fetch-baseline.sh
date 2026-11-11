#!/usr/bin/env bash
# Write the record a benchmark run compares against to $2, for the
# kind $1 (compile or runtime): BASE_SHA's from the benchmarks
# branch, else the newest master record there. Writes nothing when
# there is neither, which compare.py reports as no baseline yet.
set -euo pipefail

kind="$1"
out="$2"
branch="benchmarks"
ref="refs/remotes/origin/${branch}"

if ! git fetch --quiet --depth=1 origin "+refs/heads/${branch}:${ref}" 2>/dev/null; then
  echo "No ${branch} branch yet, so there is nothing to compare against."
  exit 0
fi

if [[ -n "${BASE_SHA:-}" ]] && git cat-file -e "${ref}:records/${BASE_SHA}/${kind}.json" 2>/dev/null; then
  git show "${ref}:records/${BASE_SHA}/${kind}.json" > "$out"
  echo "Comparing against the ${kind} record of ${BASE_SHA}."
elif git cat-file -e "${ref}:latest/${kind}.json" 2>/dev/null; then
  git show "${ref}:latest/${kind}.json" > "$out"
  echo "::warning::No ${kind} record for ${BASE_SHA:-the base commit}; comparing against the newest master record."
else
  echo "The ${branch} branch holds no ${kind} record yet."
fi
