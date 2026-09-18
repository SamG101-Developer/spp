#!/usr/bin/env bash
# Scan the repository for committed secrets and write SARIF
# for the Security tab.
set -euo pipefail

output="${1:-gitleaks.sarif}"
mode="${2:-dir}"

case "$mode" in
  dir)
    # The working tree at this commit.
    scan=(dir .)
    ;;
  git)
    # Every reachable commit. Nightly only: rewriting history is
    # the only way to clear a finding here, so it cannot gate a push.
    scan=(git . --log-opts=--all)
    ;;
  *)
    echo "::error::unknown scan mode '${mode}'; expected 'dir' or 'git'"
    exit 1
    ;;
esac

status=0
gitleaks "${scan[@]}" --report-format sarif --report-path "$output" --redact --verbose || status=$?

case "$status" in
  0)
    echo "gitleaks found no secrets"
    ;;
  1)
    echo "::error::gitleaks found committed secrets; see the SARIF results in the Security tab"
    echo "  treat every match as live: rotate the credential first, then remove it from the tree"
    echo "  a false positive belongs in .gitleaks.toml as an allowlist entry, with a comment saying why"
    ;;
  *)
    echo "::error::gitleaks failed to complete (exit ${status}); the scan result is unknown, not clean"
    exit "$status"
    ;;
esac

# Both surviving paths are meant to have written a report.
if ! [ -f "$output" ]; then
  echo "::error::gitleaks exited ${status} but wrote no report to ${output}"
  exit 1
fi

exit "$status"
