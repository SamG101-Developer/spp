#!/usr/bin/env bash
# Build the pinned gdb release into SPP_CI_GDB_PREFIX, checksum
# first, with Python (the std's pretty-printers need it). Only
# gdb itself: no simulator, gdbserver docs or translations.
# Its build dependencies come from install-debuggers-linux.sh.
set -euo pipefail

source .github/scripts/lib/verified-fetch.sh
url="https://ftp.gnu.org/gnu/gdb/gdb-${GDB_VERSION}.tar.xz"
work="${RUNNER_TEMP}/gdb"

rm -rf "$work"
mkdir -p "$work"
verified_fetch "$url" "${work}/gdb.tar.xz" "$GDB_SHA256"
tar xJf "${work}/gdb.tar.xz" -C "$work" --strip-components=1

# The system python3, not the one setup-python put first on
# PATH: gdb links its libpython, which has to be there at run
# time too, on a cache hit as well.
(
  cd "$work"
  ./configure --prefix="$SPP_CI_GDB_PREFIX" --with-python=/usr/bin/python3 \
    --without-guile --disable-sim --disable-werror --disable-nls --quiet MAKEINFO=true
  make --quiet -j "$(nproc)" all-gdb MAKEINFO=true
  make --quiet install-gdb MAKEINFO=true
)
"${SPP_CI_GDB_PREFIX}/bin/gdb" --version | head -1
