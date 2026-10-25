#!/usr/bin/env bash
# Install a cross gcc and its /usr/<triple> sysroot (args: "aarch64-linux-gnu" [package, if not gcc-<triple>]).
set -euo pipefail

# With recommends: the target's libc (crt files, libm) is only a recommendation of its gcc.
sudo apt-get update -qq
sudo apt-get install -y "${2:-gcc-$1}"
ls "/usr/$1/lib/Scrt1.o" > /dev/null
