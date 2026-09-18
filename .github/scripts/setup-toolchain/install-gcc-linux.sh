#!/usr/bin/env bash
# Install the requested GCC from Homebrew, which the Linux images
# carry already (installed, but off PATH), and point CC/CXX at it.
#
# ppa:ubuntu-toolchain-r/test is not used for GCC any more: it only
# ever publishes dated trunk snapshots, and the newest gcc-16 it has
# is from March 2026, months before the modules fix in PR125768 that
# this project needs. Homebrew carries releases.
set -euo pipefail

major="${GCC_VERSION%%.*}"

brew_bin=/home/linuxbrew/.linuxbrew/bin/brew
[ -x "$brew_bin" ] || brew_bin="$(command -v brew || true)"
if [ -z "$brew_bin" ]; then
  echo "::error::no homebrew on this runner to install GCC ${GCC_VERSION} from" >&2
  exit 1
fi

eval "$("$brew_bin" shellenv)"
export HOMEBREW_NO_ENV_HINTS=1
export HOMEBREW_NO_INSTALL_CLEANUP=1
brew update --quiet

# The newest major is plain "gcc"; the ones behind it keep a "gcc@N".
formula="gcc@${major}"
brew info --formula "$formula" > /dev/null 2>&1 || formula="gcc"
brew install --quiet "$formula"

prefix="$(brew --prefix "$formula")"
cc="${prefix}/bin/gcc-${major}"
cxx="${prefix}/bin/g++-${major}"
if [ ! -x "$cc" ] || [ ! -x "$cxx" ]; then
  echo "::error::homebrew ${formula} installed no gcc-${major}/g++-${major} under ${prefix}/bin" >&2
  exit 1
fi

# One version per formula, so a pin Homebrew cannot serve has to fail
# here rather than quietly build with whatever it does have.
full="$("$cxx" -dumpfullversion)"
case "${full}." in
  "${GCC_VERSION}".*) ;;
  *)
    echo "::error::homebrew ${formula} is GCC ${full}, not the pinned ${GCC_VERSION}" >&2
    exit 1
    ;;
esac

# The build, and everything it produces, run against this GCC's
# libstdc++ rather than the system one it is ahead of.
libstdcxx="$(readlink -f "$("$cxx" -print-file-name=libstdc++.so)")"
{
  echo "CC=${cc}"
  echo "CXX=${cxx}"
  echo "LD_LIBRARY_PATH=$(dirname "$libstdcxx")${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
} >> "$GITHUB_ENV"
