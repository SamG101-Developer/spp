#!/usr/bin/env bash
# Put the pinned osv-scanner release on PATH, checksum
# first.
set -euo pipefail

source .github/scripts/lib/verified-fetch.sh
url="https://github.com/google/osv-scanner/releases/download/v${OSV_SCANNER_VERSION}/osv-scanner_linux_amd64"
bin="$HOME/.tools/bin"

mkdir -p "$bin"
verified_fetch "$url" "$bin/osv-scanner" "$OSV_SCANNER_SHA256"
chmod +x "$bin/osv-scanner"
echo "$bin" >> "$GITHUB_PATH"
