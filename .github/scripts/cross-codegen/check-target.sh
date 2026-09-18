#!/usr/bin/env bash
# Emit the standard library for one target and check that what
# came out describes that target, rather than the host's object
# under a different folder name. Emitting is the whole test:
# linking a foreign object needs a cross linker and a cross-built
# sppc runtime, neither of which exists, so the compiler stops at
# the object for a non-host target.
#
# The corpus is a fresh `spp init` project - the STL, which its
# [vcs] section clones, plus the entry point an object needs.
#
# Usage: check-target.sh <triple> <expected-arch-substring> <expected-datalayout-prefix>
set -euo pipefail
source .github/scripts/lib/scratch-project.sh

TRIPLE="$1"
EXPECT_ARCH="$2"
EXPECT_LAYOUT="$3"

SPP="${PWD}/build/spp"
if ! [ -x "$SPP" ]; then
  echo "::error::spp binary not found at ${SPP}"
  exit 1
fi

# One name for every target: the triple's dashes are not a usable
# module folder, and each target is its own job anyway.
scratch_project "$SPP" cross

echo "::group::spp build --target ${TRIPLE}"
"$SPP" build -m rel --target "$TRIPLE" 2>&1 | tr '\r' '\n' | tail -25
echo "::endgroup::"

OUT="out/${TRIPLE}/rel"
OBJ="${OUT}/llvm/spp.o"
IR="${OUT}/llvm/lto.ll"

# Published so the workflow's failure artefact does not have to
# restate where the build put its output.
echo "ir=${PWD}/${IR}" >> "${GITHUB_OUTPUT:-/dev/null}"

if ! [ -f "$OBJ" ]; then
  echo "::error::no object emitted for ${TRIPLE} at ${OBJ}"
  exit 1
fi

# `file` reads the ELF header, so this is the arch and endianness
# as the backend wrote them, not as the triple claimed: it catches
# a 64-bit assumption on i686 or a little-endian one on s390x. The
# pattern is a regex because `file` renames machines between
# releases - 5.46 reports i386 where 5.45 reported 80386.
DESC="$(file -b "$OBJ")"
echo "object: ${DESC}"
if ! grep -qiE -- "$EXPECT_ARCH" <<< "$DESC"; then
  echo "::error::${OBJ} is not ${EXPECT_ARCH}: ${DESC}"
  exit 1
fi

# A wrong pointer width or endianness here means every offset was
# computed against the wrong target, which the header check above
# would not catch.
LAYOUT="$(grep -m1 '^target datalayout' "$IR" || true)"
echo "layout: ${LAYOUT}"
if ! grep -q -- "$EXPECT_LAYOUT" <<< "$LAYOUT"; then
  echo "::error::datalayout for ${TRIPLE} does not contain '${EXPECT_LAYOUT}': ${LAYOUT}"
  exit 1
fi

echo "ok: ${TRIPLE}"
