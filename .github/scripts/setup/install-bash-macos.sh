#!/usr/bin/env bash
# macOS ships bash 3.2, the last GPLv2 release, and the CI
# scripts use bash 4+ (mapfile, ${x,,}). Put Homebrew's bash
# first on PATH for every later step, alone in its own
# directory so no other Homebrew tool moves ahead of the
# system's. Runs under 3.2 itself, so keep it to that.
set -euo pipefail

export HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_INSTALL_CLEANUP=1
brew list --formula bash > /dev/null 2>&1 || brew install bash

bin_dir="${RUNNER_TEMP}/bash-bin"
mkdir -p "$bin_dir"
ln -sf "$(brew --prefix bash)/bin/bash" "${bin_dir}/bash"
echo "$bin_dir" >> "$GITHUB_PATH"
"${bin_dir}/bash" --version | head -1
