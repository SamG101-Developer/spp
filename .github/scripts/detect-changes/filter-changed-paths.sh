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

# Detect changed files between the base and head commits, and check if any of them match the pattern of files that can
# affect a build, test or analysis result. If any do, report `code=true`, otherwise report `code=false`.
files=$(git diff --name-only "$BASE_SHA" "$HEAD_SHA")
echo "Changed files:"
echo "$files"

# Keep this pattern in step with the directories the build actually reads. Any changes to headers, sources, tests or
# main.cpp invokes a new build. So does the workflow configs because they workflow outputs may change.
pattern='^(headers/|sources/|tests/|main\.cpp$|CMakeLists\.txt$'
pattern+='|\.cppcheck-suppressions$|\.github/(workflows|actions|scripts)/)'

# Check for a grep output and return the flag if a code change has been detected by the modified files.
if echo "$files" | grep -qE "$pattern"; then
  echo "code=true" >> "$GITHUB_OUTPUT"
else
  echo "code=false" >> "$GITHUB_OUTPUT"
fi
