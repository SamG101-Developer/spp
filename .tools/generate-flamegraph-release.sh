#!/usr/bin/env bash
set -euo pipefail

# Run the release compiler under perf and render a flamegraph into a timestamped html file.
# The timestamp is epoch seconds, so names sort lexicographically in chronological order.
#
# Release is what ships, so this is the profile that decides what is worth optimising. The debug build that
# generate-flamegraph.sh profiles spends a large share of its time in allocation, shared_ptr traffic and one-line
# accessors that -O3 inlines away, which reorders the ranking. Output is named separately for that reason: a
# debug flamegraph and a release one are not comparable to each other.
#
# It builds into its own directory because the profile needs two things the normal release build does not have:
# debug info to name frames, and frame pointers for perf to walk the stack with (gcc drops those above -O1).

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PERF_DATA="$(mktemp -t perf.data.XXXXXX)"
OUTPUT="${ROOT}/flamegraph-release-$(date +%s).html"
readonly ROOT PERF_DATA OUTPUT
readonly BUILD_DIR="${ROOT}/cmake-build-release-perf-wsl-dev"
readonly REFERENCE_CACHE="${ROOT}/cmake-build-release-wsl-dev/CMakeCache.txt"
readonly BINARY="${BUILD_DIR}/spp"
readonly FLAMEGRAPH_DIR="${ROOT}/../FlameGraph"

# Release runs several times faster than debug, so the default sample rate is raised to keep the stack count in the
# same range. Override for a longer or shorter workload.
readonly FREQ="${SPP_PERF_FREQ:-999}"

trap 'rm -f "${PERF_DATA}"' EXIT

# Read a setting from the normal release build rather than repeating it here, so the two cannot drift apart.
cache_value() {
  local key="$1"
  sed -n "s|^${key}:[^=]*=||p" "${REFERENCE_CACHE}" | head -1
}

if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
  if [[ ! -f "${REFERENCE_CACHE}" ]]; then
    echo "No ${REFERENCE_CACHE} to take the compiler settings from; configure the normal release build first." >&2
    exit 1
  fi

  cmake -S "${ROOT}" -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$(cache_value CMAKE_C_COMPILER)" \
    -DCMAKE_CXX_COMPILER="$(cache_value CMAKE_CXX_COMPILER)" \
    -DLLVM_DIR="$(cache_value LLVM_DIR)" \
    -DSPP_BUILD_TESTS=OFF \
    -DCMAKE_CXX_FLAGS="-g -fno-omit-frame-pointer"
fi

# Always build: profiling a stale binary reads as a change that did nothing.
cmake --build "${BUILD_DIR}" --target spp

perf record --call-graph fp -F "${FREQ}" -o "${PERF_DATA}" "${BINARY}" "$@"
perf script -i "${PERF_DATA}" \
    | "${FLAMEGRAPH_DIR}/stackcollapse-perf.pl" \
    | "${FLAMEGRAPH_DIR}/flamegraph.pl" > "${OUTPUT}"

echo "Wrote ${OUTPUT}"
