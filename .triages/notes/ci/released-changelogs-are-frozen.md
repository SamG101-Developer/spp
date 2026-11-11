---
name: released-changelogs-are-frozen
description: "changelog/<v>.md for v <= the base branch's VERSION cannot change; notes go in the head VERSION's entry; check-version.sh fails otherwise"
updated: 2026-11-09
---

Every code PR bumps `VERSION` and brings `changelog/<new version>.md`, which becomes the GitHub release body when
post_merge publishes it. An entry for a version at or below the base branch's `VERSION` belongs to a release that is
published, or is published by the base. Since 2026-11-09 `.github/scripts/release/check-version.sh` (the Version &
Changelog job) fails if this branch edits, removes or renames one, compared against the merge base so a fix made on the
base branch itself is not counted. Files not named like a version (`changelog/README.md`) are not checked.

**Why:** the maintainer caught a CI change written into `changelog/0.5.0.md` while master was at 0.5.0 and the branch
at 0.6.0; editing a published entry leaves the release body and the file disagreeing.

**How to apply:** a changelog line for this branch's work goes in `changelog/$(cat VERSION).md`. To fix a typo in an
old entry, change the published release body too; the check will still fail, so that needs a deliberate exception.
