#!/usr/bin/env bash
# Compose a build-tree cache key and publish it as the
# `prefix` and `key` step outputs. The only place a key is
# spelled out: the compile action and _cross_codegen.yaml
# write the trees and _sonar.yaml restores one, and a key
# written separately in each drifts silently into a
# permanent cold build.
set -euo pipefail

# The manual lever, read from .github/dependencies.toml so
# prune-caches.sh sees the same families. Bump it only for
# what the fingerprint cannot observe: a tree poisoned by a
# bad run, or a change to what is cached rather than how it
# is built.
generation="$(python3 .github/scripts/lib/pins.py get "cache.${FAMILY}.generation")"

# The toolchain as it is on this runner, not as the key
# names it: `gcc16` stays readable while the image drifts
# from 16.1 to 16.2 underneath it, and a module BMI built by
# one is not readable by the other.
COMPILER_REPORTED_VERSION="$("${CXX:-}" --version 2>&1 | head -1 || true)"
export COMPILER_REPORTED_VERSION

# One digest over everything that invalidates a tree without
# changing the readable part of the key. Fixed order and
# fixed list: a variable unset on this platform contributes
# an empty line rather than disappearing.
fingerprint="$(
  python3 - <<'PY'
import hashlib
import os

PARTS = (
    "FILES_HASH",
    "COMPILER_REPORTED_VERSION",
    "CMAKE_VERSION",
    "NINJA_VERSION",
    "LLVM_LIB_VERSION",
    "LLVM_WIN_SHA256",
    "LLVM_MAC_SHA256",
)

joined = "\n".join(os.environ.get(name, "") for name in PARTS)
print(hashlib.sha256(joined.encode()).hexdigest()[:12])
PY
)"

# The compiler version is glued to the family, not separated
# from it, because a runner-default compiler contributes no
# version at all: `msvc`, not `msvc-`. A caller with one
# tree per configuration rather than per named variant adds
# nothing.
prefix="${FAMILY}-${generation}-${OS}-${COMPILER}${COMPILER_VERSION}"
if [ -n "${VARIANT:-}" ]; then
  prefix="${prefix}-${VARIANT}"
fi

# The fingerprint sits inside the prefix so a change to it
# is a clean miss: restore-keys must not reach a tree built
# from different inputs. `prefix` is that restore-key - the
# newest tree for this variant on this branch, then on the
# base branch - and `key` pins the commit, so a re-run
# restores its own tree rather than its parent's.
prefix="${prefix}-${fingerprint}"

{
  echo "prefix=${prefix}-"
  echo "key=${prefix}-${COMMIT_SHA}"
} >> "$GITHUB_OUTPUT"

echo "build tree cache key: ${prefix}-${COMMIT_SHA}"
echo "  fingerprint over: files=${FILES_HASH:0:12} compiler=${COMPILER_REPORTED_VERSION:-unknown}" \
  "cmake=${CMAKE_VERSION:-} ninja=${NINJA_VERSION:-} llvm=${LLVM_LIB_VERSION:-}"
