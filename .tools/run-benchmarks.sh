#!/usr/bin/env bash
set -euo pipefail

# Run the benchmark suites locally, as CI's _benchmarks.yaml does: compile time (benchmarks/run.py) and run time, S++
# against C and Rust (benchmarks/runtime.py), each reported by benchmarks/compare.py with the S++/C/Rust parity table.
#
# Each record is compared against a baseline:
#   --against last   (default) this machine's previous run, so instruction counts are comparable too
#   --against ci     the newest master record on the benchmarks branch (CI's CPU: counters and sizes only)
#   --against none   just report
# Records and report.md are kept in <build>/benchmark-results; a run replaces the "last" records only when it finishes.
#
#   run-benchmarks.sh [--compile|--runtime] [--cachegrind] [--rounds N] [--against last|ci|none] [--spp PATH]
#                     [--only NAME ...]    compile workload names (hello) and/or run-time tier/name (programs/nbody)

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly ROOT

spp="${ROOT}/cmake-build-release-wsl-dev/spp"
suites=(compile runtime)
against=last
rounds=()
cachegrind=()
compile_only=()
runtime_only=()

usage() {
  sed -n '3,15p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
  exit "${1:-0}"
}

while (($#)); do
  case "$1" in
    --compile) suites=(compile) ;;
    --runtime) suites=(runtime) ;;
    --cachegrind) cachegrind=(--cachegrind) ;;
    --rounds) rounds=(--rounds "$2"); shift ;;
    --against) against="$2"; shift ;;
    --spp) spp="$(realpath "$2")"; shift ;;
    --only)
      shift
      while (($#)) && [[ "$1" != --* ]]; do
        if [[ "$1" == */* ]]; then runtime_only+=("$1"); else compile_only+=("$1"); fi
        shift
      done
      continue
      ;;
    -h|--help) usage ;;
    *) echo "Unknown option $1" >&2; usage 1 ;;
  esac
  shift
done

[[ "${against}" =~ ^(last|ci|none)$ ]] || { echo "--against takes last, ci or none" >&2; exit 1; }
[[ -x "${spp}" ]] || { echo "No spp at ${spp}; build the release compiler first." >&2; exit 1; }

# A filter naming only one kind runs only that suite.
if ((${#compile_only[@]})) && ! ((${#runtime_only[@]})); then suites=(compile); fi
if ((${#runtime_only[@]})) && ! ((${#compile_only[@]})); then suites=(runtime); fi

if ((${#cachegrind[@]})) && ! command -v valgrind > /dev/null; then
  echo "--cachegrind needs valgrind on PATH." >&2
  exit 1
fi

RESULTS="$(dirname "${spp}")/benchmark-results"
REPORT="${RESULTS}/report.md"
readonly RESULTS REPORT
mkdir -p "${RESULTS}"
: > "${REPORT}"

# The std pin is taken from the project's own clone when it has the commit, which saves mirroring it over the network.
std_git=()
std_rev="$(< "${ROOT}/benchmarks/std.rev")"
if git -C "${ROOT}/project/vcs/std" cat-file -e "${std_rev}^{commit}" 2> /dev/null; then
  std_git=(--std-git "${ROOT}/project/vcs/std")
fi

baseline_for() {
  local kind="$1" out="$2"
  rm -f "${out}"
  case "${against}" in
    last) [[ -f "${RESULTS}/${kind}.json" ]] && cp "${RESULTS}/${kind}.json" "${out}" ;;
    ci) (cd "${ROOT}" && BASE_SHA="$(git merge-base HEAD origin/master 2> /dev/null || true)" \
          bash .github/scripts/bench/fetch-baseline.sh "${kind}" "${out}") ;;
  esac
  return 0
}

echo "spp: ${spp} ($(git -C "${ROOT}" rev-parse --short HEAD)$(git -C "${ROOT}" diff --quiet HEAD -- sources headers || echo ', with local changes'))"
echo "Comparing against: ${against}"

for kind in "${suites[@]}"; do
  record="${RESULTS}/${kind}.new.json"
  baseline="${RESULTS}/${kind}.baseline.json"
  if [[ "${kind}" == compile ]]; then
    echo -e "\n== Compile time"
    filter=(); ((${#compile_only[@]})) && filter=(--workloads "${compile_only[@]}")
    python3 "${ROOT}/benchmarks/run.py" --spp "${spp}" --out "${record}" \
      ${rounds[@]+"${rounds[@]}"} ${cachegrind[@]+"${cachegrind[@]}"} ${std_git[@]+"${std_git[@]}"} ${filter[@]+"${filter[@]}"}
  else
    echo -e "\n== Run time (S++, C, Rust)"
    filter=(); ((${#runtime_only[@]})) && filter=(--benchmarks "${runtime_only[@]}")
    python3 "${ROOT}/benchmarks/runtime.py" --spp "${spp}" --out "${record}" \
      ${rounds[@]+"${rounds[@]}"} ${cachegrind[@]+"${cachegrind[@]}"} ${std_git[@]+"${std_git[@]}"} ${filter[@]+"${filter[@]}"}
  fi

  baseline_for "${kind}" "${baseline}"
  compare=(--record "${record}" --summary "${REPORT}" --report-only)
  [[ -f "${baseline}" ]] && compare+=(--baseline "${baseline}")
  python3 "${ROOT}/benchmarks/compare.py" "${compare[@]}"

  # A filtered run is partial, so it does not replace the last full record.
  if ((${#compile_only[@]} + ${#runtime_only[@]})); then
    mv "${record}" "${RESULTS}/${kind}.partial.json"
  else
    mv "${record}" "${RESULTS}/${kind}.json"
  fi
  rm -f "${baseline}"
done

echo "Report: ${REPORT}"
