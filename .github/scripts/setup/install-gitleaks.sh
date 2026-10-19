#!/usr/bin/env bash
# Put the pinned gitleaks release on PATH, checksum first.
set -euo pipefail
source .github/scripts/lib/verified-fetch.sh

verified_install_bin \
  "https://github.com/gitleaks/gitleaks/releases/download/v${GITLEAKS_VERSION}/gitleaks_${GITLEAKS_VERSION}_linux_x64.tar.gz" \
  "$GITLEAKS_SHA256" gitleaks gitleaks
