#!/usr/bin/env bash
# Install the pinned Doxygen release into SPP_DOXYGEN_PREFIX,
# checksum first.
set -euo pipefail

source .github/scripts/lib/verified-fetch.sh
url="https://github.com/doxygen/doxygen/releases/download/Release_${DOXYGEN_VERSION//./_}/doxygen-${DOXYGEN_VERSION}.linux.bin.tar.gz"
verified_fetch "$url" "${RUNNER_TEMP}/doxygen.tar.gz" "$DOXYGEN_SHA256"
mkdir -p "$SPP_DOXYGEN_PREFIX"
tar xzf "${RUNNER_TEMP}/doxygen.tar.gz" -C "$SPP_DOXYGEN_PREFIX" --strip-components=1
