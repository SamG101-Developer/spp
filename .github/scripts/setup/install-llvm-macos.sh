#!/usr/bin/env bash
# macOS takes its whole toolchain - compiler, libc++ and the
# LLVM dev libraries - from brew. The release tarball's LLVM
# is built without RTTI, so a TU instantiating a polymorphic
# LLVM template (ValueMap) needs typeinfo its libraries lack;
# brew's is built with RTTI, like apt.llvm.org's on Linux.
# brew has no ld64.lld in llvm; it is the lld formula.
set -euo pipefail
source .github/scripts/lib/retry.sh

major="$LLVM_LIB_VERSION"
prefix="$SPP_CI_LLVM_MAC_PREFIX"

# The unversioned formula is the newest major; once it moves
# past [pin.llvm-lib], that major is llvm@<major>.
formula=llvm
current="$(brew info --json=v2 llvm | python3 -c 'import json, sys; print(json.load(sys.stdin)["formulae"][0]["versions"]["stable"])')"
if [ "${current%%.*}" != "$major" ]; then
  formula="llvm@${major}"
fi
lld=lld
[ "$formula" = llvm ] || lld="lld@${major}"

retry brew install --quiet "$formula" "$lld"

# One prefix holds everything the rest of CI looks for: brew's
# keg, plus ld64.lld from the lld keg beside clang, where
# -fuse-ld=lld looks first.
ln -sfn "$(brew --prefix "$formula")" "$prefix"
ln -sf "$(brew --prefix "$lld")/bin/ld64.lld" "${prefix}/bin/ld64.lld"

# A missing piece surfaces much later as something inscrutable:
# a configure that falls back to the system clang, a link that
# cannot read the archives.
for needed in bin/clang++ bin/clang-scan-deps bin/ld64.lld lib/cmake/llvm/LLVMConfig.cmake lib/libLLVMCore.a; do
  if [ ! -e "${prefix}/${needed}" ]; then
    echo "::error::brew's ${formula} has no ${needed}; macOS cannot build against it" >&2
    exit 1
  fi
done
version="$("${prefix}/bin/llvm-config" --version)"
if [ "${version%%.*}" != "$major" ]; then
  echo "::error::brew installed LLVM ${version}, not the [pin.llvm-lib] major ${major}" >&2
  exit 1
fi
if [ "$("${prefix}/bin/llvm-config" --has-rtti)" != YES ]; then
  echo "::error::brew's LLVM ${version} is built without RTTI; spp's TUs need its typeinfo" >&2
  exit 1
fi
"${prefix}/bin/clang++" --version
