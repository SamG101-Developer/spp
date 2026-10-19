#!/usr/bin/env bash
# Build the pinned valgrind release into SPP_VALGRIND_PREFIX,
# checksum first.
set -euo pipefail

source .github/scripts/lib/verified-fetch.sh
url="https://sourceware.org/pub/valgrind/valgrind-${VALGRIND_VERSION}.tar.bz2"
work="${RUNNER_TEMP}/valgrind"

rm -rf "$work"
mkdir -p "$work"
verified_fetch "$url" "${work}/valgrind.tar.bz2" "$VALGRIND_SHA256"
tar xjf "${work}/valgrind.tar.bz2" -C "$work" --strip-components=1

(
  cd "$work"
  ./configure --prefix="$SPP_VALGRIND_PREFIX" --quiet
  make --quiet -j "$(nproc)"
  make --quiet install
)
"${SPP_VALGRIND_PREFIX}/bin/valgrind" --version
