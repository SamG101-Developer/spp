---
name: commit-subject-format
description: Commit subjects use the PR title format, checked by a commit-msg hook that reads pr_title.yaml
updated: 2026-11-17
---

Commit subjects follow the PR title format, `type(optional scope): description`, with a lowercase description
matching `^[a-z][a-z0-9 ./:-]*$`. The `commit-msg` hook (`.github/precommit/commit_msg.py`) reads the types and the
regex from `.github/workflows/pr_title.yaml`. Merge, `Revert "`, `fixup!`, `squash!` and `amend!` subjects are exempt,
and a trailing ` (#N)` or ` [skip ci]` is ignored.

**Why:** one source for the rule, so commits and PR titles can't drift apart.

**How to apply:** add or remove a type only in `pr_title.yaml`. The hook only runs once `pre-commit install` has been
run (via `default_install_hook_types`), and it is client-side only: CI doesn't check commit messages.
