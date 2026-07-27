#!/usr/bin/env bash
# General setup for the OSV scanner package. Put the latest osv-scanner release on PATH.
set -euo pipefail

# Install the package from GitHub, and move the binary into a common location. This is the one install that does not go
# to an SPP_*_PREFIX: the scanner is fetched fresh every run and never cached, so /usr/local/bin buys a slot on PATH
# without a GITHUB_PATH step. It is also the only install needing sudo.
curl -sSLf -o osv-scanner \
  https://github.com/google/osv-scanner/releases/latest/download/osv-scanner_linux_amd64
chmod +x osv-scanner
sudo mv osv-scanner /usr/local/bin/
