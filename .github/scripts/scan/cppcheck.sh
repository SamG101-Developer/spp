#!/usr/bin/env bash
# Run cppcheck over the project and fail on anything it
# reports. Writes SARIF for the Security tab and echoes a
# readable form of it for the job log.
set -euo pipefail

output="${1:-cppcheck.sarif}"
jobs="${2:-$(nproc)}"

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
  --output-format=sarif \
  --output-file="$output" \
  headers sources main.cpp || status=$?

# SARIF is for the Security tab, not for reading in a log.
if [ -f "$output" ]; then
  python3 - "$output" <<'PY'
import json
import sys

for run in json.load(open(sys.argv[1])).get("runs", []):
    for result in run.get("results", []):
        where = result.get("locations", [{}])[0].get("physicalLocation", {})
        path = where.get("artifactLocation", {}).get("uri", "?")
        line = where.get("region", {}).get("startLine", 0)
        print(f"{path}:{line}: {result.get('ruleId', '?')}: {result['message']['text']}")
PY
fi

case "$status" in
  0)
    echo "cppcheck found no issues"
    ;;
  2)
    echo "::error::cppcheck reported findings; see the log above and the Security tab"
    echo "  a false positive belongs in .cppcheck-suppressions, or inline as // cppcheck-suppress <id>"
    exit 1
    ;;
  *)
    echo "::error::cppcheck failed to complete (exit ${status}); the result is unknown, not clean"
    exit "$status"
    ;;
esac
