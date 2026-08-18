#!/usr/bin/env bash
# Run cppcheck over the project and fail on anything it
# reports.
set -euo pipefail

jobs="${1:-$(nproc)}"

status=0
cppcheck \
  --enable=warning,performance,portability \
  --inline-suppr \
  --suppressions-list=.cppcheck-suppressions \
  --check-level=exhaustive \
  --std=c++26 \
  --language=c++ \
  --error-exitcode=2 \
  --quiet \
  -j "$jobs" \
  -Iheaders \
  --template='{file}:{line}:{column}: {severity}: {message} [{id}]' \
  --template-location='{file}:{line}:{column}: note: {info}' \
  headers sources main.cpp || status=$?

case "$status" in
  0)
    echo "cppcheck found no issues"
    ;;
  2)
    echo "::error::cppcheck reported findings; see the log above"
    echo "  a false positive belongs in .cppcheck-suppressions, or inline as // cppcheck-suppress <id>, with a"
    echo "  comment saying why it is not a real defect"
    exit 1
    ;;
  *)
    echo "::error::cppcheck failed to complete (exit ${status}); the result is unknown, not clean"
    exit "$status"
    ;;
esac
