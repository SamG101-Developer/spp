#!/usr/bin/env bash
# Scan the repository for committed secrets and write SARIF
# for the Security tab.
#
# This exists because the gitleaks pre-commit hook does not
# cover CI. That hook scans the index, and under
# `pre-commit run --all-files` on a fresh checkout nothing
# is staged, so it passes over a repository it never read.
# A hook is also bypassable with `--no-verify`, which is
# exactly the wrong property for the one check that catches
# a live credential.
#
# Unlike the dependency scan, findings here are fatal. A CVE
# in a dependency is something to schedule; a key in the
# tree is something to rotate before the branch merges.
set -euo pipefail

output="${1:-gitleaks.sarif}"
mode="${2:-dir}"

case "$mode" in
  dir)
    # The working tree at this commit: what the repository
    # holds now, which is what has to come back clean.
    scan=(dir .)
    ;;
  git)
    # Every reachable commit, which needs the full history
    # the caller checked out. Reserved for the nightly run:
    # rewriting history is the only way to clear a finding
    # here, so it is not a per-push gate.
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

# Both surviving paths are meant to have written a report,
# so its absence means the scanner exited without doing the
# one thing it was asked to do.
if ! [ -f "$output" ]; then
  echo "::error::gitleaks exited ${status} but wrote no report to ${output}"
  exit 1
fi

exit "$status"
