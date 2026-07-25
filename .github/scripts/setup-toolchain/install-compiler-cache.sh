#!/usr/bin/env bash
# Install the compiler cache this runner uses: ccache everywhere but Windows,
# which gets sccache because ccache cannot drive cl/clang-cl.
set -euo pipefail

case "$RUNNER_OS" in
  Linux)   sudo apt-get install -y ccache ;;
  macOS)   brew install ccache ninja ;;
  Windows) choco install sccache -y ;;
esac
