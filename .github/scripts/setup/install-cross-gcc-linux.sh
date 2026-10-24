#!/usr/bin/env bash
# Install a cross gcc and its /usr/<triple> sysroot (args: "aarch64-linux-gnu" [package, if not gcc-<triple>]).
set -euo pipefail

sudo apt-get update -qq
sudo apt-get install -y --no-install-recommends "${2:-gcc-$1}"
ls "/usr/$1/lib" > /dev/null
