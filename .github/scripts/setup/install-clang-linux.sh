#!/usr/bin/env bash
# Install the requested Clang, plus the libstdc++ it
# borrows, and point CC/CXX at it.
#
# The libstdc++ is Homebrew's GCC, the one the GCC lane builds
# with: ppa:ubuntu-toolchain-r/test only has a March 2026 trunk
# snapshot of gcc-16 (see install-gcc-linux.sh), and its server
# going down (503s) failed every Clang lane. Clang does not
# search Homebrew, so a config file beside it names the GCC,
# for every clang and clang++ run (the sibling libraries'
# builds included), and configure.sh passes it too, so the
# compile commands clang-tidy reads carry it.
set -euo pipefail
source .github/scripts/lib/retry.sh

retry sudo "${RUNNER_TEMP}/llvm.sh" "${CLANG_VERSION}"

# Installs Homebrew's GCC and writes its CC/CXX/COMPILER_PATH to
# GITHUB_ENV; CC/CXX are replaced below, and COMPILER_PATH (its
# binutils) stays, so clang links with the same "ld" as GCC.
bash .github/scripts/setup/install-gcc-linux.sh
gcc_cxx="$(sed -n 's/^CXX=//p' "$GITHUB_ENV" | tail -1)"
gcc_dir="$(dirname "$("$gcc_cxx" -print-libgcc-file-name)")"
if [ ! -f "${gcc_dir}/crtbegin.o" ]; then
  echo "::error::no GCC installation (crtbegin.o) at ${gcc_dir}" >&2
  exit 1
fi

# Clang reads "<driver>.cfg" from its own (resolved) bin directory:
# clang.cfg for clang-N, clang++.cfg for clang++-N.
llvm_bin="$(dirname "$(readlink -f "$(command -v "clang++-${CLANG_VERSION}")")")"
for driver in clang clang++; do
  echo "--gcc-install-dir=${gcc_dir}" | sudo tee "${llvm_bin}/${driver}.cfg" > /dev/null
done

# Check clang took it: otherwise it silently picks the image's own,
# older GCC.
for driver in "clang-${CLANG_VERSION}" "clang++-${CLANG_VERSION}"; do
  selected="$("$driver" -v -fsyntax-only -x c /dev/null 2>&1 | sed -n 's/^Selected GCC installation: //p')"
  if [ "$selected" != "$gcc_dir" ]; then
    echo "::error::${driver} selected GCC '${selected}', not Homebrew's ${gcc_dir}" >&2
    exit 1
  fi
done

{
  echo "CC=clang-${CLANG_VERSION}"
  echo "CXX=clang++-${CLANG_VERSION}"
  echo "SPP_CI_GCC_INSTALL_DIR=${gcc_dir}"
} >> "$GITHUB_ENV"
