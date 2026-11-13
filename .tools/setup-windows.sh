#!/usr/bin/env bash
# Set up a Windows machine to build spp the way CI's Windows MSVC lane does, with CI's own scripts: the pinned LLVM,
# the libraries it needs (zlib, zstd, libxml2, pthreads4w), the small libraries, MSVC as the compiler; then, if asked,
# configure and build into build/. CI's runner variables are stood in for: RUNNER_OS, RUNNER_ARCH, RUNNER_TEMP, and
# the GITHUB_ENV and GITHUB_PATH files each step appends to, read back after it as Actions does.
#
# Needs, installed once: Visual Studio 2026 Build Tools ("Desktop development with C++"), Git for Windows, CMake 4.5
# or newer, Ninja, Python 3, all on PATH. Run it from Git Bash started inside the "x64 Native Tools Command Prompt
# for VS" (or "ARM64 Native Tools" on an ARM machine), at the root of a clone made with core.autocrlf=false.
#
#   .tools/setup-windows.sh [--configure | --build] [--build-type Release|Debug|RelWithDebInfo]
#
# What it installs goes where CI puts it, under your home: ~/llvm, ~/llvm-deps and ~/.local. A rerun skips what is
# there already, so it is quick, and --build then rebuilds.

set -euo pipefail

mode=deps
build_type=Release
while (( $# > 0 )); do
  case "$1" in
    --configure) mode=configure ;;
    --build) mode=build ;;
    --build-type)
      build_type="${2:?--build-type needs a value}"
      shift
      ;;
    *)
      echo "usage: .tools/setup-windows.sh [--configure | --build] [--build-type <type>]" >&2
      exit 2
      ;;
  esac
  shift
done

cd "$(dirname "${BASH_SOURCE[0]}")/.."

fail() {
  echo "setup-windows: $*" >&2
  exit 1
}

case "$(uname -s)" in
  MINGW* | MSYS* | CYGWIN*) ;;
  *) fail "this is for Windows (Git Bash); on Linux and macOS, follow the README" ;;
esac

# Scripts checked out with CRLF line endings fail in bash ($'\r': command not found).
if grep -q $'\r' .github/scripts/lib/load-versions.sh; then
  fail "this clone has CRLF line endings. Re-clone with: git clone -c core.autocrlf=false <url>"
fi

# The tools CI's steps call, each on PATH.
command -v cl > /dev/null \
  || fail "cl (MSVC) is not on PATH: start Git Bash from the \"x64 Native Tools Command Prompt for VS\""
for tool in cmake ninja git curl tar; do
  command -v "${tool}" > /dev/null || fail "${tool} is not on PATH"
done

# CI's scripts call python3, which a Windows Python install names python: a shim on PATH stands in.
RUNNER_TEMP="$(mktemp -d)"
trap 'rm -rf "${RUNNER_TEMP}"' EXIT
if ! command -v python3 > /dev/null; then
  command -v python > /dev/null || fail "Python 3 is not on PATH"
  mkdir -p "${RUNNER_TEMP}/shim"
  printf '#!/usr/bin/env bash\nexec python "$@"\n' > "${RUNNER_TEMP}/shim/python3"
  chmod +x "${RUNNER_TEMP}/shim/python3"
  PATH="${RUNNER_TEMP}/shim:${PATH}"
fi

# The runner variables. The architecture is the machine's, asked of Windows: Git Bash itself may be x64 under
# emulation on an ARM machine, and reports that.
arch="$(powershell -NoProfile -NonInteractive -Command \
  '[System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture' | tr -d '\r')"
case "${arch}" in
  X64) RUNNER_ARCH=X64 ;;
  Arm64) RUNNER_ARCH=ARM64 ;;
  *) fail "no LLVM release is pinned for Windows on ${arch}" ;;
esac
export RUNNER_OS=Windows RUNNER_ARCH RUNNER_TEMP
export GITHUB_ENV="${RUNNER_TEMP}/github-env" GITHUB_PATH="${RUNNER_TEMP}/github-path"
: > "${GITHUB_ENV}"
: > "${GITHUB_PATH}"

# What a step appended to GITHUB_ENV and GITHUB_PATH, made this shell's, as Actions does before the next step. Every
# value the scripts write is a single line (KEY=VALUE).
take_env() {
  local line
  while IFS= read -r line || [[ -n "${line}" ]]; do
    [[ -n "${line}" ]] && export "${line?}"
  done < "${GITHUB_ENV}"
  while IFS= read -r line || [[ -n "${line}" ]]; do
    [[ -n "${line}" ]] && PATH="$(cygpath -u "${line}"):${PATH}"
  done < "${GITHUB_PATH}"
  : > "${GITHUB_ENV}"
  : > "${GITHUB_PATH}"
}

step() {
  echo "==> $1"
  bash "$2"
  take_env
}

step "Pins (.github/dependencies.toml)" .github/scripts/lib/load-versions.sh

# CI restores these two from its caches, and their scripts do the whole download or build each time, so a prefix
# that already holds what they install is left as it is.
if [[ -f "$(cygpath -u "${SPP_CI_LLVM_WIN_PREFIX}")/lib/cmake/llvm/LLVMConfig.cmake" ]]; then
  echo "==> LLVM ${LLVM_WIN_TAG#llvmorg-}: already in ${SPP_CI_LLVM_WIN_PREFIX}"
else
  step "LLVM ${LLVM_WIN_TAG#llvmorg-} (into ${SPP_CI_LLVM_WIN_PREFIX})" .github/scripts/setup/install-llvm-windows.sh
fi
if [[ -f "$(cygpath -u "${SPP_CI_LLVM_DEPS_WIN_PREFIX}")/lib/cmake/pthreads4w/pthreads4w-config.cmake" ]]; then
  echo "==> zlib, zstd, libxml2, pthreads4w: already in ${SPP_CI_LLVM_DEPS_WIN_PREFIX}"
else
  step "zlib, zstd, libxml2, pthreads4w (into ${SPP_CI_LLVM_DEPS_WIN_PREFIX})" \
    .github/scripts/setup/install-llvm-deps-windows.sh
fi
step "Small libraries (into ${SPP_CI_LOCAL_PREFIX})" .github/scripts/setup/install-small-libs.sh
step "Prefixes for CMake" .github/scripts/setup/expose-prefixes.sh
COMPILER=msvc step "MSVC as the compiler" .github/scripts/setup/select-windows-compiler.sh

# The environment a later configure or build needs, kept for a new shell.
env_file="build/windows.env"
mkdir -p build
{
  echo "# Written by .tools/setup-windows.sh; source it in a new Git Bash before building by hand."
  printf 'export CC=%q CXX=%q CMAKE_PREFIX_PATH=%q\n' "${CC}" "${CXX}" "${CMAKE_PREFIX_PATH}"
} > "${env_file}"
echo "==> Dependencies ready; their environment is in ${env_file}"

if [[ "${mode}" == configure || "${mode}" == build ]]; then
  echo "==> Configure (${build_type}, into build/)"
  BUILD_TYPE="${build_type}" EXTRA_FLAGS="" SPP_CI_NO_COMPILER_LAUNCHER=1 bash .github/scripts/build/configure.sh
fi
if [[ "${mode}" == build ]]; then
  echo "==> Build"
  bash .github/scripts/build/build.sh
  echo "==> Built: build/spp.exe and build/tests/spp_tests.exe"
fi
