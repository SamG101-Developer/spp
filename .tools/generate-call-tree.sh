#!/usr/bin/env bash
set -euo pipefail

# Run the compiler under perf and render a call tree into a timestamped svg file.
# The timestamp is epoch seconds, so names sort lexicographically in chronological order.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PERF_DATA="$(mktemp -t perf.data.XXXXXX)"
OUTPUT="${ROOT}/calltree-$(date +%s).svg"
readonly ROOT PERF_DATA OUTPUT
readonly BINARY="${ROOT}/cmake-build-debug-wsl-dev/spp"

trap 'rm -f "${PERF_DATA}"' EXIT

perf record -g -F 99 -o "${PERF_DATA}" "${BINARY}" "$@"
perf script -i "${PERF_DATA}" \
    | c++filt \
    | gprof2dot -f perf --strip --wrap --node-thres=1 --edge-thres=1 \
    | dot -Tsvg -Gsplines=ortho -Grankdir=LR -Gconcentrate=true -o "${OUTPUT}"

echo "Wrote ${OUTPUT}"
