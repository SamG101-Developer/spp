#!/usr/bin/env bash
# Decide whether this ref changed anything that can affect a build, test or analysis result, and report it as the `code`
# step output.
set -euo pipefail

# No usable base (manual dispatch, merge queue, new branch, force push to an unknown commit): fall back to building,
# because we cannot prove it is safe to skip.
if [ -z "$BASE_SHA" ] \
  || [ "$BASE_SHA" = "0000000000000000000000000000000000000000" ] \
  || ! git cat-file -e "${BASE_SHA}^{commit}" 2>/dev/null; then
  echo "no usable base commit; assuming code changed"
  echo "code=true" >> "$GITHUB_OUTPUT"
  exit 0
fi

# Detect changed files between the base and head commits, and decide from them whether anything could have moved a
# build, test or analysis result.
files=$(git diff --name-only "$BASE_SHA" "$HEAD_SHA")
echo "Changed files:"
echo "$files"

# The inert directories contain changes that don't affect the build, test or analysis results. The inert files are
# generally either documentation or configuration.
inert='^(docs/|\.vale/|\.idea/|[^/]*\.md$|\.gitignore$)'

# grep -v exits 0 as soon as one line fails to match, i.e. as soon as one changed path is not inert.
if [ -z "$files" ]; then
  echo "no files changed"
  echo "code=false" >> "$GITHUB_OUTPUT"
elif echo "$files" | grep -qvE "$inert"; then
  echo "code=true" >> "$GITHUB_OUTPUT"
else
  echo "code=false" >> "$GITHUB_OUTPUT"
fi
