#!/usr/bin/env bash
# Install the pinned Doxygen release into SPP_DOXYGEN_PREFIX, checksum first.
set -euo pipefail

url="https://github.com/doxygen/doxygen/releases/download/Release_${DOXYGEN_VERSION//./_}/doxygen-${DOXYGEN_VERSION}.linux.bin.tar.gz"
curl -sSLf -o "${RUNNER_TEMP}/doxygen.tar.gz" "$url"
echo "${DOXYGEN_SHA256}  ${RUNNER_TEMP}/doxygen.tar.gz" | sha256sum -c -
mkdir -p "$SPP_DOXYGEN_PREFIX"
tar xzf "${RUNNER_TEMP}/doxygen.tar.gz" -C "$SPP_DOXYGEN_PREFIX" --strip-components=1
