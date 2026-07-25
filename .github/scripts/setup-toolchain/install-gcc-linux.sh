#!/usr/bin/env bash
# Install the requested GCC from the toolchain PPA and point CC/CXX at it.
set -euo pipefail

sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt-get install -y "gcc-${GCC_VERSION}" "g++-${GCC_VERSION}"
echo "CC=gcc-${GCC_VERSION}" >> "$GITHUB_ENV"
echo "CXX=g++-${GCC_VERSION}" >> "$GITHUB_ENV"
