#!/usr/bin/env bash
# Build the corpus for one target and check that what came
# out actually describes that target, rather than the host's
# object under a different folder name. Emitting is the
# whole test: linking a foreign object needs a cross linker
# and a cross-built sppc runtime, neither of which exists.
#
# Usage: check-target.sh <triple> <expected-arch-substring> <expected-datalayout-prefix>
set -euo pipefail

TRIPLE="$1"
EXPECT_ARCH="$2"
EXPECT_LAYOUT="$3"

SPP="${PWD}/build/spp"
PROJECT="${PWD}/project"

if ! [ -x "$SPP" ]; then
  echo "::error::spp binary not found at ${SPP}"
  exit 1
fi

# The compiler resolves its project relative to its own
# location when given no subcommand, but "build" runs
# against the working directory, so this has to be the
# project root.
cd "$PROJECT"

echo "::group::spp build --target ${TRIPLE}"
"$SPP" build -m rel --target "$TRIPLE" 2>&1 | tr '\r' '\n' | tail -25
echo "::endgroup::"

OUT="out/${TRIPLE}/rel"
OBJ="${OUT}/llvm/spp.o"
IR="${OUT}/llvm/lto.ll"

if ! [ -f "$OBJ" ]; then
  echo "::error::no object emitted for ${TRIPLE} at ${OBJ}"
  exit 1
fi

# What the object says it is. "file" reads the ELF header,
# so this is the arch and the endianness as the backend
# actually wrote them, not as the triple claimed. This is
# the check that catches a 64-bit assumption on i686 or a
# little-endian one on s390x.
DESC="$(file -b "$OBJ")"
echo "object: ${DESC}"
if ! grep -qi -- "$EXPECT_ARCH" <<<"$DESC"; then
  echo "::error::${OBJ} is not ${EXPECT_ARCH}: ${DESC}"
  exit 1
fi

# And what the IR says the layout is. A wrong pointer width
# or endianness here means every offset the compiler computed
# was computed against the wrong target, which an object-header
# check alone would not catch.
LAYOUT="$(grep -m1 '^target datalayout' "$IR" || true)"
echo "layout: ${LAYOUT}"
if ! grep -q -- "$EXPECT_LAYOUT" <<<"$LAYOUT"; then
  echo "::error::datalayout for ${TRIPLE} does not contain '${EXPECT_LAYOUT}': ${LAYOUT}"
  exit 1
fi

echo "ok: ${TRIPLE}"
