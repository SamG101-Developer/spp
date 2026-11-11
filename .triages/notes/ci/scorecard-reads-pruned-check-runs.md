---
name: scorecard-reads-pruned-check-runs
description: "Scorecard's CI-Tests reads Actions check runs on merged PR heads; nightly prune-runs (retain_days: 3) removes them, so it scores 0/6"
updated: 2026-11-01
---
Scorecard's CI-Tests check looks for successful check runs on the head commit of recently merged PRs. The `prune-runs` job in `nightly.yaml` deletes workflow runs older than 3 days, and their check runs seem to go with them. On 2026-11-01 the last six merged PRs each had 0 workflow runs and 0 `github-actions` check runs on their heads, while open PR #135 had 21. Check runs from `github-advanced-security` (CodeQL, gitleaks, OSV, Cppcheck) survive, so they do not show that CI ran.

**Why:** the 0/6 score says nothing about whether CI ran on those PRs. The same pruning also removes the evidence that it did.

**How to apply:** do not read this alert as missing CI. The retention has to cover the span of Scorecard's sample of merged PRs for the score to move, and it moves only as new PRs merge. See [[python-codeql-alerts-frozen]].
