#!/usr/bin/env bash
# Install and size this runner's compiler cache, or turn it
# off. Windows gets sccache, which can drive cl and clang-cl;
# ccache cannot. Off, configure.sh reads
# SPP_NO_COMPILER_LAUNCHER and leaves
# CMAKE_<LANG>_COMPILER_LAUNCHER unset.
set -euo pipefail

if [ "${1:-on}" = off ]; then
  {
    echo "CCACHE_DISABLE=1"
    echo "SPP_NO_COMPILER_LAUNCHER=1"
  } >> "$GITHUB_ENV"
  exit 0
fi

case "$RUNNER_OS" in
  Linux) sudo apt-get install -y ccache ;;
  macOS) brew install ccache ;;
  Windows) choco install sccache -y ;;
esac

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
