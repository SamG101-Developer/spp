#!/usr/bin/env bash
set -euo pipefail

# Run clang-tidy with the repo's .clang-tidy over the clang build tree, writing de-duplicated findings to
# clang-tidy-report.txt at the repo root. Arguments are file regexes (default: every project file).
# Needs a current clang build: clang-tidy loads that tree's module files. Exits 1 when clang-tidy found something.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG="$(mktemp -t clang-tidy.XXXXXX)"
readonly ROOT LOG
readonly BUILD_DIR="${ROOT}/cmake-build-release-wsl-clang"
readonly REPORT="${ROOT}/clang-tidy-report.txt"

trap 'rm -f "${LOG}"' EXIT

if (($# == 0)); then
    set -- "^${ROOT}/(headers|sources|tests|main\.cpp)"
fi

# Progress lines go to the terminal; the findings are read back from the log.
run-clang-tidy-23 -p "${BUILD_DIR}" -clang-tidy-binary clang-tidy-23 -j "$(nproc)" -quiet "$@" 2>&1 \
    | tee "${LOG}" \
    | grep --line-buffered -E '^\[ *[0-9]+/[0-9]+\]' || true

# A header's findings repeat once per file that includes it.
grep -E '^/.+:[0-9]+:[0-9]+: (warning|error): .*\]$' "${LOG}" | sort -u > "${REPORT}" || true

sed -E 's/.*\[([^]]+)\]$/\1/' "${REPORT}" | sort | uniq -c | sort -rn
echo "Wrote ${REPORT} ($(wc -l < "${REPORT}") findings)"
[[ ! -s "${REPORT}" ]]
