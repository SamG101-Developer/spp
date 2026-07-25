#!/usr/bin/env bash
# Publish the pinned toolchain and cache-key versions from .github/versions.env into the job environment, plus today's
# UTC date for the cache keys that rotate daily, plus the install prefixes below. Lines in versions.env not of the form
# NAME=value are ignored.
set -euo pipefail

{
  grep -E '^[A-Za-z_][A-Za-z0-9_]*=' .github/versions.env
  echo "CACHE_DATE=$(date -u +%Y-%m-%d)"

  # Install prefixes. Everything the project installs for itself goes under $HOME: no step needs sudo, and the tree can
  # be handed straight to actions/cache. They are not in versions.env because that file is copied into GITHUB_ENV, where
  # $HOME would not expand.
  #
  #   SPP_LOCAL_PREFIX     the small CMake libraries, cached as one tree (spp-libs-*)
  #   SPP_DOXYGEN_PREFIX   doxygen, cached on its own by release version (doxygen-*)
  #   SPP_LLVM_WIN_PREFIX  the Windows clang+llvm tarball, cached by version (llvm-win-*)
  #
  # The standalone tools live under ~/.tools rather than inside ~/.local so that no cache encloses another: the spp-libs
  # entry saves all of ~/.local, and a job that both populated it and installed a tool underneath would hand the two
  # keys the same files to fight over.
  #
  # Paths not listed here are not ours to choose: apt.llvm.org installs to /usr/lib/llvm-<version>, brew reports its own
  # prefix, and the prebuilt Boost tarball unpacks to $HOME/boost.
  #
  # Workflow `path:` keys deliberately keep the literal `~/...` spelling instead of these variables: actions/cache
  # expands `~` itself on every runner, whereas the value below is a POSIX path that a Windows runner would not resolve.
  echo "SPP_LOCAL_PREFIX=$HOME/.local"
  echo "SPP_DOXYGEN_PREFIX=$HOME/.tools/doxygen"
  echo "SPP_LLVM_WIN_PREFIX=$HOME/llvm"
} >> "$GITHUB_ENV"
