#!/usr/bin/env bash
# Point CC/CXX at the MSVC or clang-cl driver the job asked for.
set -euo pipefail

cc=cl
[ "$COMPILER" = "clang-cl" ] && cc=clang-cl
echo "CC=$cc" >> "$GITHUB_ENV"
echo "CXX=$cc" >> "$GITHUB_ENV"
