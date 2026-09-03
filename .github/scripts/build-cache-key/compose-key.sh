#!/usr/bin/env bash
# Compose a build-tree cache key, and publish it as the
# `prefix` and `key` step outputs. This script is the only
# place a key is spelled out: _compile.yaml and
# _cross_codegen.yaml write the trees, _sonar.yaml restores
# one, and a key written separately in each is a key that
# drifts apart silently, into a permanent cold build.
set -euo pipefail

# The generation is the manual lever, read from
# .github/dependencies.toml so prune-caches.sh can see the
# same families. It only has to be bumped for the things the
# fingerprint below cannot observe: a tree poisoned by a bad
# run, or a change to what is cached rather than how it is
# built.
generation="$(python3 .github/scripts/lib/pins.py get "cache.${FAMILY}.generation")"

# The toolchain as it actually is on this runner, not as the
# key names it. `gcc16` in the key stays readable while the
# image drifts from 16.1 to 16.2 underneath it, and a module
# BMI built by one is not readable by the other - so the
# reported version is what the fingerprint tracks.
COMPILER_REPORTED_VERSION="$("${CXX:-}" --version 2>&1 | grep -oE '[0-9]+(\.[0-9]+)+' | head -1 || true)"
export COMPILER_REPORTED_VERSION

# One short digest over everything that invalidates a tree
# without changing the readable part of the key: the files
# that define the build, the compiler's real version, and the
# pinned versions of the tools that write into the tree.
fingerprint="$(
  python3 - <<'PY'
import hashlib
import os

# Fixed order and fixed list: a variable that is unset on
# this platform contributes an empty line rather than
# disappearing, so the digest stays stable across runs of
# the same job.
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
# version at all: `msvc`, not `msvc-`.
prefix="${FAMILY}-${generation}-${OS}-${COMPILER}${COMPILER_VERSION}"

# A caller with one tree per configuration, rather than one
# per named variant, has nothing to add here.
if [ -n "${VARIANT:-}" ]; then
  prefix="${prefix}-${VARIANT}"
fi

# The fingerprint sits inside the prefix, not after the sha,
# so that a change to it is a clean miss: restore-keys must
# not be able to reach a tree built from different inputs.
prefix="${prefix}-${fingerprint}"

# `prefix` is the restore-keys fallback: it picks the newest
# tree for this variant on this branch, then on the base
# branch. `key` pins the exact commit, so a re-run of the
# same sha restores its own tree rather than its parent's.
{
  echo "prefix=${prefix}-"
  echo "key=${prefix}-${COMMIT_SHA}"
} >> "$GITHUB_OUTPUT"

echo "build tree cache key: ${prefix}-${COMMIT_SHA}"
echo "  fingerprint over: files=${FILES_HASH:0:12} compiler=${COMPILER_REPORTED_VERSION:-unknown}" \
  "cmake=${CMAKE_VERSION:-} ninja=${NINJA_VERSION:-} llvm=${LLVM_LIB_VERSION:-}"
