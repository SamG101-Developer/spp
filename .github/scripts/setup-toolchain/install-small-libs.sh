#!/usr/bin/env bash
# The header/CMake dependencies: check each out at a pinned
# commit and install it into SPP_LOCAL_PREFIX, which the
# calling step caches as a single tree. This is for all
# libraries except for LLVM and Boost (different installation).
set -euo pipefail
source .github/scripts/lib/llvm-prefix.sh

PREFIX="$SPP_LOCAL_PREFIX"
llvm_dir="$(llvm_prefix)"

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
  export CMAKE_PREFIX_PATH="$PREFIX;${BOOST_ROOT};$llvm_dir"
else
  export CMAKE_PREFIX_PATH="$PREFIX${BOOST_ROOT:+:$BOOST_ROOT}:$llvm_dir"
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
    -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=lib \
    -DBUILD_TESTING=OFF "$@"
  cmake --build "$name/build" --target install
}

# One stamp per library, each holding the exact manifest
# record it was installed from. The combined stamp below says
# whether the prefix is complete; these say which parts of it
# are still current, so bumping one library rebuilds one
# library rather than all ten.
stamp_dir="$PREFIX/.spp-libs.d"
mkdir -p "$stamp_dir"

# Every library, its commit and its flags come from the
# manifest, in the order listed there. Read on descriptor 3
# so that nothing inside the loop can consume the record
# stream by reading stdin. The raw record is kept as read,
# rather than reassembled from the fields, so that comparing
# it against a stamp cannot drift on the tab handling.
while IFS= read -r -u 3 record; do
  IFS=$'\t' read -r name repo commit flagstr <<< "$record"

  # Anything about the library that could change what gets
  # installed - its commit, its repo, its flags - is in the
  # record, so an unchanged record means an unchanged install.
  stamp="$stamp_dir/$name"
  if [ -f "$stamp" ] && [ "$(cat "$stamp")" = "$record" ]; then
    echo "${name} @ ${commit} already installed"
    continue
  fi

  # Word-splitting the flags is the point; pins.py rejects a
  # flag containing whitespace so that this stays safe.
  read -ra flags <<< "$flagstr"
  cmake_install "$name" "$repo" "$commit" ${flags[@]+"${flags[@]}"}
  printf '%s' "$record" > "$stamp"
done 3<<< "$records"

# A library dropped from the manifest leaves its headers and
# CMake config behind, which nothing looks for once the
# find_package() is gone. Only the stamp is cleaned up, so
# that re-adding the library reinstalls it rather than
# trusting whatever the prefix still holds.
while IFS= read -r stamp; do
  name="$(basename "$stamp")"
  if ! printf '%s\n' "$records" | cut -f1 | grep -qxF "$name"; then
    echo "::warning::${name} is no longer in the manifest; its installed files remain in the prefix"
    rm -f "$stamp"
  fi
done < <(find "$stamp_dir" -maxdepth 1 -type f)

# Written last, and read back by check-small-libs.sh: the
# prefix only counts as installed once every record above
# has been through cmake_install or been vouched for by its
# own stamp.
printf '%s\n' "$records" > "$PREFIX/.spp-libs-stamp"
