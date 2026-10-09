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

# Homebrew's GCC is configured against Homebrew's binutils and emits
# directives, ".base64" among them, that the assembler on the runner
# image (2.42) does not know.
brew install --quiet "$formula" binutils

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

# COMPILER_PATH is where GCC looks for "as" and "ld", and is read
# before PATH, so only the compiler's own subprograms move - nothing
# else on the runner sees a different binutils.
binutils="$(brew --prefix binutils)/bin"
if [ ! -x "${binutils}/as" ]; then
  echo "::error::homebrew binutils installed no assembler under ${binutils}" >&2
  exit 1
fi

# SPP_STATIC_RUNTIME links libstdc++.a; -print-file-name echoes the bare name when it is missing.
if [ "$("$cxx" -print-file-name=libstdc++.a)" = "libstdc++.a" ]; then
  echo "::error::homebrew ${formula} ships no libstdc++.a; SPP_STATIC_RUNTIME cannot link" >&2
  exit 1
fi

# Homebrew's specs file appends "--dynamic-linker <brew>/lib/ld.so" and
# "-rpath <brew>/lib" to every link, which ties the binaries to this
# runner's Homebrew. Drop both (and the blank line after each, as gcc
# rejects a run of them); the header and -L additions stay.
specs="$(dirname "$("$cxx" -print-libgcc-file-name)")/specs"
if [ -f "$specs" ]; then
  awk '
    gap && /^$/ { gap = 0; next }
    { gap = 0 }
    held != "" {
      h = held; held = ""
      if ((h == "*link:" && /^\+ --dynamic-linker /) || (h == "*homebrew_rpath:" && /^-rpath /)) { gap = 1; next }
      print h
    }
    $0 == "*link:" || $0 == "*homebrew_rpath:" { held = $0; next }
    { gsub(/ %\(homebrew_rpath\) /, " "); print }
  ' "$specs" > "${specs}.spp" && mv "${specs}.spp" "$specs"
  if grep -q -e '--dynamic-linker /home/linuxbrew' -e '^-rpath ' -e 'homebrew_rpath' "$specs"; then
    echo "::error::could not strip the Homebrew rpath/loader from ${specs}" >&2
    exit 1
  fi
fi

{
  echo "CC=${cc}"
  echo "CXX=${cxx}"
  echo "COMPILER_PATH=${binutils}"
} >> "$GITHUB_ENV"
