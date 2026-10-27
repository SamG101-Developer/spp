#!/usr/bin/env bash
# Install a cross gcc and its /usr/<triple> sysroot (args: "aarch64-linux-gnu" [package, if not gcc-<triple>]).
# Package "arm-gnu" is Arm's own release instead, for a target Ubuntu packages no gcc for (aarch64_be): it
# carries its own sysroot, exported as SPP_QEMU_SYSROOT, and its gcc goes on PATH as <triple>-gcc, the
# name spp links with.
set -euo pipefail

if [ "${2:-}" != arm-gnu ]; then
  # With recommends: the target's libc (crt files, libm) is only a recommendation of its gcc.
  sudo apt-get update -qq
  sudo apt-get install -y "${2:-gcc-$1}"
  ls "/usr/$1/lib/Scrt1.o" > /dev/null
  exit 0
fi

source .github/scripts/lib/verified-fetch.sh
arm="${1/-linux-/-none-linux-}"
key="ARM_GNU_SHA256_$(tr 'a-z_-' 'A-Z__' <<< "${1%%-*}")"
url="https://gitlab.arm.com/api/v4/projects/tooling%2Fgnu-toolchains-for-arm/packages/generic/gnu-toolchain/${ARM_GNU_VERSION}/arm-gnu-toolchain-${ARM_GNU_VERSION}-x86_64-${arm}.tar.xz"
root="${RUNNER_TEMP}/arm-gnu"

rm -rf "$root"
mkdir -p "$root/wrap"
verified_fetch "$url" "${RUNNER_TEMP}/arm-gnu.tar.xz" "${!key:-}"
tar -xJf "${RUNNER_TEMP}/arm-gnu.tar.xz" -C "$root" --strip-components=1

printf '#!/bin/sh\nexec %q "$@"\n' "${root}/bin/${arm}-gcc" > "${root}/wrap/$1-gcc"
chmod +x "${root}/wrap/$1-gcc"
sysroot="$("${root}/bin/${arm}-gcc" -print-sysroot)"
ls "$("${root}/bin/${arm}-gcc" -print-file-name=Scrt1.o)" > /dev/null

echo "${root}/wrap" >> "$GITHUB_PATH"
echo "SPP_QEMU_SYSROOT=${sysroot}" >> "$GITHUB_ENV"
"${root}/bin/${arm}-gcc" --version | head -1
