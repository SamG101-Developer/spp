#!/usr/bin/env bash
# Turn the compiler cache off; configure-cmake reads SPP_NO_COMPILER_LAUNCHER and leaves CMAKE_<LANG>_COMPILER_LAUNCHER
# unset.
set -euo pipefail

{
  echo "CCACHE_DISABLE=1"
  echo "SPP_NO_COMPILER_LAUNCHER=1"
} >> "$GITHUB_ENV"
