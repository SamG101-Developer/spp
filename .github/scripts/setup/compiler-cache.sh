#!/usr/bin/env bash
# Point this runner's compiler cache at RUNNER_TEMP, or turn it
# off. ccache-action installs and sizes it and reads these
# dirs, as does any server restarted mid-build. Off,
# configure.sh reads SPP_NO_COMPILER_LAUNCHER and leaves
# CMAKE_<LANG>_COMPILER_LAUNCHER unset.
set -euo pipefail

if [ "${1:-on}" = off ]; then
  {
    echo "CCACHE_DISABLE=1"
    echo "SPP_NO_COMPILER_LAUNCHER=1"
  } >> "$GITHUB_ENV"
  exit 0
fi

{
  echo "CCACHE_DIR=${RUNNER_TEMP}/ccache"
  echo "SCCACHE_DIR=${RUNNER_TEMP}/sccache"
} >> "$GITHUB_ENV"
