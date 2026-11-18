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

# Homebrew keeps libstdc++ and libgcc_s beside the GCC, not in the
# directory clang reads from --gcc-install-dir ("cannot find -lgcc_s").
# The runtime is linked statically, as the GCC lane's is
# (SPP_STATIC_RUNTIME): the runner's own libstdc++.so.6 is older than
# GCC 16's, so a dynamically linked program could not load it.
runtime_dir="$(dirname "$("$gcc_cxx" -print-file-name=libstdc++.a)")"
if [ ! -f "${runtime_dir}/libstdc++.a" ]; then
  echo "::error::homebrew's GCC has no libstdc++.a (asked: ${runtime_dir})" >&2
  exit 1
fi

# Clang reads "<driver>.cfg" from its own (resolved) bin directory:
# clang.cfg for clang-N, clang++.cfg for clang++-N. The link options
# are fenced off as unused when only compiling, which -Werror would
# otherwise fail.
llvm_bin="$(dirname "$(readlink -f "$(command -v "clang++-${CLANG_VERSION}")")")"
for driver in clang clang++; do
  printf -- '--gcc-install-dir=%s\n--start-no-unused-arguments -L%s -static-libgcc -static-libstdc++ --end-no-unused-arguments\n' \
    "$gcc_dir" "$runtime_dir" | sudo tee "${llvm_bin}/${driver}.cfg" > /dev/null
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

# And that it builds and runs a C and a C++ program (an exception
# included) with -Werror, as CMake's first check does: a broken link
# fails here rather than inside configure.
probe="$(mktemp -d)"
printf 'int main(void) { return 0; }\n' > "${probe}/c.c"
printf '#include <stdexcept>\nint main() { try { throw std::runtime_error("x"); } catch (...) { return 0; } return 1; }\n' \
  > "${probe}/cxx.cpp"
"clang-${CLANG_VERSION}" -Werror "${probe}/c.c" -o "${probe}/c"
"clang++-${CLANG_VERSION}" -Werror "${probe}/cxx.cpp" -o "${probe}/cxx"
"${probe}/c"
"${probe}/cxx"
rm -rf "$probe"

{
  echo "CC=clang-${CLANG_VERSION}"
  echo "CXX=clang++-${CLANG_VERSION}"
  echo "SPP_CI_GCC_INSTALL_DIR=${gcc_dir}"
} >> "$GITHUB_ENV"
