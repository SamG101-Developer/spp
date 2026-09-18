#!/usr/bin/env bash
# The header/CMake dependencies: check each out at a pinned
# commit and install it into SPP_LOCAL_PREFIX, which the
# calling step caches as one tree. LLVM and Boost come
# prebuilt and are installed elsewhere.
set -euo pipefail
source .github/scripts/lib/llvm-prefix.sh

PREFIX="$SPP_LOCAL_PREFIX"
llvm_dir="$(llvm_prefix)"

# Read before leaving the repository root: pins.py resolves the
# manifest relative to the working directory.
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

cmake_install() {
  local name="$1" url="$2" sha="$3"
  shift 3

  # GitHub serves any reachable commit to a depth-1 fetch, so a
  # pinned checkout costs what a shallow clone does while naming
  # exactly what gets built.
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

# One stamp per library, each holding the manifest record it was
# installed from, so bumping one library rebuilds one library.
stamp_dir="$PREFIX/.spp-libs.d"
mkdir -p "$stamp_dir"

# Read on descriptor 3 so nothing inside the loop can consume the
# record stream from stdin. The raw record is kept as read, so
# comparing it against a stamp cannot drift on tab handling.
while IFS= read -r -u 3 record; do
  IFS=$'\t' read -r name repo commit flagstr <<< "$record"

  # Everything that could change the install is in the record.
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

# A dropped library leaves its headers behind, which nothing looks
# for once the find_package() is gone. Only the stamp goes, so
# re-adding it reinstalls rather than trusting the prefix.
while IFS= read -r stamp; do
  name="$(basename "$stamp")"
  if ! printf '%s\n' "$records" | cut -f1 | grep -qxF "$name"; then
    echo "::warning::${name} is no longer in the manifest; its installed files remain in the prefix"
    rm -f "$stamp"
  fi
done < <(find "$stamp_dir" -maxdepth 1 -type f)

# Written last, and read back by check-small-libs.sh.
printf '%s\n' "$records" > "$PREFIX/.spp-libs-stamp"
