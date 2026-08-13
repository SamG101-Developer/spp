#!/usr/bin/env bash
# Scan the repository's dependency manifests and write SARIF for the Security tab. The osv-scanner distinguishes
# "found vulnerabilities" from "could not scan". Findings are reported rather than fatal. A scanner that failed to run
# is considered fatal.
# Todo: To make findings blocking -> change the `1)` branch below to exit 1.
set -euo pipefail

output="${1:-osv.sarif}"

status=0
osv-scanner scan --recursive --format sarif --output "$output" ./ || status=$?

case "$status" in
  0)
    echo "osv-scanner found no known vulnerabilities"
    ;;
  1)
    echo "::warning::osv-scanner reported known vulnerabilities; see the SARIF results in the Security tab"
    ;;
  *)
    echo "::error::osv-scanner failed to complete (exit ${status}); the scan result is unknown, not clean"
    exit "$status"
    ;;
esac

# Both surviving paths are meant to have written a report, so its absence means the scanner exited successfully
# without doing the one thing it was asked to do.
if ! [ -f "$output" ]; then
  echo "::error::osv-scanner exited ${status} but wrote no report to ${output}"
  exit 1
fi
