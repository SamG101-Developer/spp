#!/usr/bin/env bash
# Scan the dependency manifests and write SARIF for the Security
# tab. osv-scanner distinguishes "found vulnerabilities" from
# "could not scan": findings are reported, a scanner that could
# not run is fatal.
set -euo pipefail

output="${1:-osv.sarif}"

status=0
osv-scanner scan --recursive --config osv-scanner.toml --format sarif --output-file "$output" ./ || status=$?

case "$status" in
  0)
    echo "osv-scanner found no known vulnerabilities"
    ;;
  1)
    echo "::warning::osv-scanner reported known vulnerabilities; see the SARIF results in the Security tab"
    exit 1
    ;;
  *)
    echo "::error::osv-scanner failed to complete (exit ${status}); the scan result is unknown, not clean"
    exit "$status"
    ;;
esac

# Both surviving paths are meant to have written a report.
if ! [ -f "$output" ]; then
  echo "::error::osv-scanner exited ${status} but wrote no report to ${output}"
  exit 1
fi
