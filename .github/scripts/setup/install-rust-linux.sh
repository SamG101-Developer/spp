#!/usr/bin/env bash
# Install the pinned rustc and rust-std components into
# SPP_CI_RUST_PREFIX, checksum first. No cargo: the
# benchmarks call rustc directly.
set -euo pipefail

source .github/scripts/lib/verified-fetch.sh
triple="x86_64-unknown-linux-gnu"
work="${RUNNER_TEMP}/rust"

rm -rf "$work"
mkdir -p "$work"
for component in rustc rust-std; do
  case "$component" in
    rustc) digest="$RUST_SHA256_RUSTC" ;;
    rust-std) digest="$RUST_SHA256_RUST_STD" ;;
  esac
  name="${component}-${RUST_VERSION}-${triple}"
  verified_fetch "https://static.rust-lang.org/dist/${name}.tar.xz" "${work}/${name}.tar.xz" "$digest"
  tar xJf "${work}/${name}.tar.xz" -C "$work"
  "${work}/${name}/install.sh" --prefix="$SPP_CI_RUST_PREFIX" --disable-ldconfig > /dev/null
done
"${SPP_CI_RUST_PREFIX}/bin/rustc" --version
