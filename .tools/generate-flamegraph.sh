#!/usr/bin/env bash
set -euo pipefail

# Run the compiler under perf and render a flamegraph into a timestamped html file.
# The timestamp is epoch seconds, so names sort lexicographically in chronological order.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PERF_DATA="$(mktemp -t perf.data.XXXXXX)"
OUTPUT="${ROOT}/flamegraph-$(date +%s).html"
readonly ROOT PERF_DATA OUTPUT
readonly BINARY="${ROOT}/cmake-build-debug-wsl-dev/spp"
readonly FLAMEGRAPH_DIR="${ROOT}/../FlameGraph"

trap 'rm -f "${PERF_DATA}"' EXIT

perf record -g -F 99 -o "${PERF_DATA}" "${BINARY}" "$@"
perf script -i "${PERF_DATA}" \
    | "${FLAMEGRAPH_DIR}/stackcollapse-perf.pl" \
    | "${FLAMEGRAPH_DIR}/flamegraph.pl" > "${OUTPUT}"

echo "Wrote ${OUTPUT}"
