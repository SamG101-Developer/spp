#!/usr/bin/env bash
# Emit the standard library for one target and check that what
# came out describes that target, rather than the host's object
# under a different folder name. A Linux target is then linked,
# with its cross gcc, against the sppc the STL ships for it;
# where either is missing the link fails and only the object is
# checked, and where it succeeds the executable is checked too.
#
# The corpus is a fresh `spp init` project - the STL, which its
# [vcs] section clones, plus the entry point an object needs.
set -euo pipefail
source .github/scripts/lib/scratch-project.sh

TRIPLE="$1"
EXPECT_ARCH="$2"
EXPECT_LAYOUT="$3"
CODEGEN_OFF="${4:-}"

SPP="${PWD}/build/spp"
if ! [ -x "$SPP" ]; then
  echo "::error::spp binary not found at ${SPP}"
  exit 1
fi

# One name for every target: the triple's dashes are not a usable
# module folder, and each target is its own job anyway.
scratch_project "$SPP" cross

# Hardening the target's backend cannot emit fails the build, so
# those keys are switched off rather than skipped. A bare key is
# in [binary.codegen]; another section's is dotted.
if [ -n "$CODEGEN_OFF" ]; then
  declare -A sections=()
  for key in $CODEGEN_OFF; do
    [[ $key == *.* ]] || key="binary.codegen.${key}"
    sections[${key%.*}]+="${key##*.} = false\n"
  done
  for section in "${!sections[@]}"; do printf '\n[%s]\n%b' "$section" "${sections[$section]}" >> spp.toml; done
fi

echo "::group::spp build --target ${TRIPLE}"
LOG="$("$SPP" build -m rel --target "$TRIPLE" 2>&1 | tr '\r' '\n' || true)"
tail -25 <<< "$LOG"
echo "::endgroup::"

# The out folder is the compiler's spelling of the target, not the
# triple asked for, so it is read off the build rather than restated.
FOLDER="$(sed -n 's/^Built object for \([^:]*\):.*/\1/p' <<< "$LOG")"
if [ -z "$FOLDER" ]; then
  echo "::error::spp build --target ${TRIPLE} did not report an object"
  exit 1
fi
echo "folder: ${FOLDER}"

OUT="out/${FOLDER}/rel"
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

# Linked: the executable is checked as the object was.
EXE="${OUT}/cross"
if grep -q '^Built executable:' <<< "$LOG"; then
  EXE_DESC="$(file -b "$EXE")"
  echo "executable: ${EXE_DESC}"
  if ! grep -qiE -- "$EXPECT_ARCH" <<< "$EXE_DESC"; then
    echo "::error::${EXE} is not ${EXPECT_ARCH}: ${EXE_DESC}"
    exit 1
  fi
else
  echo "not linked (no cross linker or shipped sppc for ${FOLDER})"
fi

echo "ok: ${TRIPLE}"
