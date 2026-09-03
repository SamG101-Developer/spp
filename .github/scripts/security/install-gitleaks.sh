#!/usr/bin/env bash
# Put the pinned gitleaks release on PATH, checksum first.
set -euo pipefail

source .github/scripts/lib/verified-fetch.sh
url="https://github.com/gitleaks/gitleaks/releases/download/v${GITLEAKS_VERSION}/gitleaks_${GITLEAKS_VERSION}_linux_x64.tar.gz"
bin="$HOME/.tools/bin"
archive="${RUNNER_TEMP}/gitleaks.tar.gz"

mkdir -p "$bin"
verified_fetch "$url" "$archive" "$GITLEAKS_SHA256"

# The tarball is flat: the binary sits at its root next to
# the licence and the readme, so only the one member is
# worth unpacking.
tar -xzf "$archive" -C "$bin" gitleaks
chmod +x "$bin/gitleaks"
echo "$bin" >> "$GITHUB_PATH"
