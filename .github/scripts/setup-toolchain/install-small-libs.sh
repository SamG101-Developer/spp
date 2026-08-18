#!/usr/bin/env bash
# The header/CMake dependencies: check each out at a pinned
# commit and install it into SPP_LOCAL_PREFIX, which the
# calling step caches as a single tree. This is for all
# libraries except for LLVM and Boost (different installation).
set -euo pipefail

PREFIX="$SPP_LOCAL_PREFIX"

# Read the manifest before leaving the repository root: pins.py
# resolves .github/dependencies.toml relative to the working
# directory, and a failure here has to stop the script rather
# than leave the loop below with nothing to install.
records="$(python3 .github/scripts/lib/pins.py libraries)"
if [ -z "$records" ]; then
  echo "install-small-libs: the manifest lists no libraries" >&2
  exit 1
fi

mkdir -p "$PREFIX" "${RUNNER_TEMP}/libs"
cd "${RUNNER_TEMP}/libs"

if [ "$RUNNER_OS" = "Windows" ]; then
  PREFIX="$(cygpath -m "$PREFIX")"
  llvm_prefix="$(cygpath -m "$SPP_LLVM_WIN_PREFIX")"
  export CMAKE_PREFIX_PATH="$PREFIX;${BOOST_ROOT};$llvm_prefix"
else
  llvm_prefix="${LLVM_PREFIX:-/usr/lib/llvm-${LLVM_LIB_VERSION}}"
  export CMAKE_PREFIX_PATH="$PREFIX${BOOST_ROOT:+:$BOOST_ROOT}:$llvm_prefix"
fi

# The cmake install helper checks the repo out at the given
# commit, configures it with the provided flags (typically
# disable the tests / benchmarks), builds it with Ninja,
# and installs it into the system.
cmake_install() {
  local name="$1" url="$2" sha="$3"
  shift 3

  # A pinned checkout rather than a branch clone. GitHub
  # will serve any reachable commit to a depth-1 fetch, so
  # this costs the same as the shallow clone it replaces
  # while naming exactly what gets built.
  git init -q "$name"
  git -C "$name" remote add origin "$url"
  git -C "$name" fetch -q --depth 1 origin "$sha"
  git -C "$name" checkout -q --detach FETCH_HEAD
  echo "${name} @ ${sha}"

  cmake -S "$name" -B "$name/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DBUILD_TESTING=OFF "$@"
  cmake --build "$name/build" --target install
}

# Every library, its commit and its flags come from the
# manifest, in the order listed there. Read on descriptor 3
# so that nothing inside the loop can consume the record
# stream by reading stdin.
while IFS=$'\t' read -r -u 3 name repo commit flagstr; do
  # Word-splitting the flags is the point; pins.py rejects a
  # flag containing whitespace so that this stays safe.
  read -ra flags <<< "$flagstr"
  cmake_install "$name" "$repo" "$commit" ${flags[@]+"${flags[@]}"}
done 3<<< "$records"

# Written last, and read back by check-small-libs.sh: the
# prefix only counts as installed once every record above
# has been through cmake_install.
printf '%s\n' "$records" > "$PREFIX/.spp-libs-stamp"
