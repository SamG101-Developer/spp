#!/usr/bin/env bash
# Size the compiler cache and, on Windows, point sccache at the directory the cache step restores into.
set -euo pipefail

if [ "$RUNNER_OS" = "Windows" ]; then
  {
    echo "SCCACHE_DIR=${USERPROFILE}\\.cache\\sccache"
    echo "SCCACHE_CACHE_SIZE=500M"
  } >> "$GITHUB_ENV"
else
  {
    echo "CCACHE_MAXSIZE=500M"
    echo "CCACHE_COMPRESS=true"
  } >> "$GITHUB_ENV"
fi
