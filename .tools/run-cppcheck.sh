#!/usr/bin/env bash
set -euo pipefail

# Run cppcheck over the whole project, writing the report to cppcheck-report.txt at the repo root.
# Exits 1 when cppcheck found something, so this can gate a pre-commit hook.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly ROOT
readonly REPORT="${ROOT}/cppcheck-report.txt"

status=0
(
    cd "${ROOT}"
    cppcheck --enable=all --inline-suppr --suppressions-list=.cppcheck-suppressions --check-level=exhaustive --std="c++26" \
        --language=c++ --error-exitcode=1 \
        --template='{file}:{line}:{column}: {severity}: {message} [{id}]' --template-location='{file}:{line}:{column}: note: {info}' \
        --output-file="${REPORT}" \
        -j "$(nproc)" -I headers \
        headers sources main.cpp "$@"
) || status=$?

echo "Wrote ${REPORT}"
exit "${status}"
