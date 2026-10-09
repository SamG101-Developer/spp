#!/usr/bin/env bash
# Put the pinned osv-scanner release on PATH, checksum first.
set -euo pipefail
source .github/scripts/lib/verified-fetch.sh

verified_install_bin \
  "https://github.com/google/osv-scanner/releases/download/v${OSV_SCANNER_VERSION}/osv-scanner_linux_amd64" \
  "$OSV_SCANNER_SHA256" osv-scanner
