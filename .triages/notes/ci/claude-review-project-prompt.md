---
name: claude-review-project-prompt
description: "The PR reviewer runs Opus on .github/review/prompt.md (areas in parallel, each finding verified), not the generic code-review plugin, which went shallow on big PRs"
updated: 2026-11-18
---

`_claude_review.yaml` runs `claude-code-action` with `--model claude-opus-5-5` and a prompt that only points at
`.github/review/prompt.md`. That file holds the review: load `CLAUDE.md` and the notes indexes, split the PR by area
(analysis, codegen, driver, ci, tests, docs), review each area in a parallel `Task` subagent that reads whole functions
around the hunks, send every finding to a fresh subagent that tries to refute it, then post one inline comment per
confirmed finding and always one summary comment (so a quiet review is visibly a review). Its checklists are the
failure kinds this project has had: exception-unsafe guards, cross-repo contracts (prelude, sppc numbers, the test
runner's std names, intrinsics), check shapes the AIR passes match, per-platform runtime helpers.

**Why:** until 2026-11-18 both repos ran `code-review@claude-code-plugins` on the default Sonnet. Its bar is "definitely
fails to compile / definitely wrong", so it mostly stayed silent or flagged typos, and on PR #156 (181 files, 11k diff
lines) it skipped its own multi-agent and validation steps: one agent skimmed the diff in 8 turns for $3.19. It still
found one real regression (an unclosed `{` reported as a prelude error).

**Can't change the repo:** `--allowedTools` is read and comment only (no `Edit`/`Write`, no git writes, no `gh api`),
and `--disallowedTools` denies them explicitly as well (pushes, commits, branches, `gh pr merge`/`review`/`close`/`edit`,
`gh api`, `gh run`, ...), which holds even if the allow list grows. The job token is `contents: read`. The action's Claude
GitHub App token may have write access to code, so the tool lists are what stop the bot using it for more than comments.

**How to apply:** change what the reviewer looks for in `prompt.md`, not the workflow; add a checklist item when a bug
class bites that a reviewer could have caught. The prompt is read from the PR's own checkout, which is fine only because
the job runs for same-repo PRs. SPP-STL still runs the plugin; port this there once it has proved itself. An on-demand
deeper pass (a `/deep-review` trigger) is not set up.
