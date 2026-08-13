#!/usr/bin/env bash
# Publish the pinned toolchain and cache-key versions from .github/versions.env into the job environment, plus the
# install prefixes below. Lines in versions.env not of the form NAME=value are ignored. Note that this file is itself an
# input to the dependency cache key, because it is where SPP_LOCAL_PREFIX is chosen.
set -euo pipefail

VERSIONS=".github/versions.env"

is_allowed_key() {
  case "$1" in
    BOOST_SHA256_LINUX | BOOST_SHA256_MACOS | BOOST_SHA256_WINDOWS | BOOST_VERSION) return 0 ;;
    CMAKE_VERSION | GCC_VERSION | LIBSTDCXX_GCC_VERSION) return 0 ;;
    DOXYGEN_CACHE_VERSION | DOXYGEN_SHA256 | DOXYGEN_VERSION) return 0 ;;
    GTEST_PARALLEL_COMMIT) return 0 ;;
    LLVM_LIB_VERSION | LLVM_SH_SHA256) return 0 ;;
    LLVM_WIN_ASSET | LLVM_WIN_SHA256 | LLVM_WIN_TAG) return 0 ;;
    OSV_SCANNER_SHA256 | OSV_SCANNER_VERSION) return 0 ;;
    *) return 1 ;;
  esac
}

# Checked in full before anything is written, so a rejected file cannot leave half its keys already exported.
while IFS= read -r line; do
  key="${line%%=*}"
  value="${line#*=}"

  if ! is_allowed_key "$key"; then
    echo "::error::unexpected key '${key}' in ${VERSIONS}"
    echo "::error::If it belongs there, add it to is_allowed_key in .github/scripts/lib/load-versions.sh."
    exit 1
  fi

  # Every pin is a version number, a git tag, a commit, a digest or one release asset filename.
  if ! [[ $value =~ ^[A-Za-z0-9._:/+-]+$ ]]; then
    echo "::error::value of '${key}' in ${VERSIONS} is not a version, tag, commit, digest or asset name"
    exit 1
  fi
done < <(grep -E '^[A-Za-z_][A-Za-z0-9_]*=' "$VERSIONS")

{
  grep -E '^[A-Za-z_][A-Za-z0-9_]*=' "$VERSIONS"

  # Install prefixes. Everything the project installs for itself goes under $HOME: no step needs sudo, and the tree can
  # be handed straight to actions/cache. They are not in versions.env because that file is copied into GITHUB_ENV, where
  # $HOME would not expand.
  #
  #   SPP_LOCAL_PREFIX     the small CMake libraries, cached as one tree (spp-libs-*)
  #   SPP_DOXYGEN_PREFIX   doxygen, cached on its own by release version (doxygen-*)
  #   SPP_LLVM_WIN_PREFIX  the Windows clang+llvm tarball, cached by version (llvm-win-*)
  echo "SPP_LOCAL_PREFIX=$HOME/.local"
  echo "SPP_DOXYGEN_PREFIX=$HOME/.tools/doxygen"
  echo "SPP_LLVM_WIN_PREFIX=$HOME/llvm"
} >> "$GITHUB_ENV"
