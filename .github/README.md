# CI, rebuilt

Staging area for the reworked workflows. Nothing in here runs — GitHub only reads
`.github/workflows` — so it can sit alongside the current setup until you're happy.

All internal references are already written as `./.github/...`, so the migration is a
rename and nothing else:

```bash
git rm -r .github && mv .github2 .github && git add .github
```

## The pipeline

```
changes ─┐
lint ────┴─> cppcheck ─┬─> build-test        release ∥ asan ∥ tsan, fail-fast
                       └─> build-test-extra  clang/macOS/Windows, advisory

docs, security, sonar   independent, start immediately
claude_code_review      own trigger, own file
```

`lint` is one reusable workflow holding two parallel jobs (PR title format,
pre-commit), so `needs: [lint]` means "both passed".

`ci-success` is the single required status check. It gates on
`changes, lint, cppcheck, build-test, docs, security`; `build-test-extra` and `sonar`
are deliberately left out so they report red without blocking a merge.

## Why the old setup never compiled incrementally

Three separate reasons, all fixed here.

1. **Every "Save build tree" step called `actions/cache/restore`, not
   `actions/cache/save`.** A restore in save position is silently a no-op, so no
   build tree was ever written and every run started cold. This was in
   `_build_and_test.yaml`, `_sanitizers.yaml` and `post_merge.yaml` alike.

2. **`actions/checkout` gives every file the same fresh mtime.** Even with a tree
   restored, Ninja compares source mtimes against object mtimes, sees every source as
   newer, and rebuilds the lot. `actions/checkout/restore_mtimes.py` rewrites tracked
   files' mtimes from commit history, which makes them *stable*: a file unchanged
   since the cached build keeps an mtime older than the cached objects (Ninja skips
   it), and a file touched by a new commit gets a newer one (Ninja rebuilds exactly
   it and its dependents). Two independent clones produce byte-identical mtimes,
   which is the property the whole scheme rests on. ~0.3 s for this repo's 702 files.

3. **ccache was carrying weight it cannot carry.** It does not understand C++20 named
   modules, and this project builds most of its code through a `CXX_MODULES` file
   set. It is still wired up for the plain translation units, but the *build tree*
   cache is what actually makes CI incremental now.

`post_merge.yaml` builds the same three variants as the PR pipeline, so every cache
key a PR looks for exists on `master` — a PR branch may only read caches from its own
branch and its base, so a warm `master` is what lets a brand-new PR start warm.

## Two correctness bugs fixed along the way

- **CodeQL was restoring the compiler cache.** Its manual build mode extracts code by
  watching the compiler run, so a ccache hit means the extractor never sees that
  translation unit and the database comes out incomplete — reporting green over code
  that was never analysed. `setup-toolchain` now takes `compiler-cache: false`, and
  the CodeQL job restores no build tree either, for the same reason.
- **`llvm.sh` was downloaded twice** in `_setup_toolchain` (once for clang, once for
  the LLVM 22 dev libraries), and the second run reinstalled a toolchain the first had
  already provided.

## Deduplication

| Was | Now |
| --- | --- |
| `_build_and_test.yaml` + `_sanitizers.yaml` (~90 % identical) | `_compile.yaml`, one variant matrix |
| `changes` job copy-pasted into `pr.yaml` and `post_merge.yaml` | `actions/detect-changes` |
| bespoke `warm-cache` job in `post_merge.yaml` | `_compile.yaml`, same variants as PR |
| harden-runner + checkout repeated in every job | `actions/checkout` |
| gtest-parallel clone + invocation duplicated | `actions/run-tests` (and cached) |
| pre-commit + title inline in `pr.yaml` | `_lint.yaml` |

`pr.yaml` is now pure orchestration: the pipeline shape is readable from the `needs:`
edges alone.

## Before you flip it over

- **cppcheck is a hard gate on every severity**, as requested — `--enable=all` with
  `--error-exitcode=1`. `cpp-check-report.txt` in the repo root is currently non-empty
  (`useStlAlgorithm`, `shadowVariable`, `redundantAssignment`, …), so the gate will be
  red on the first run. Fix them, add inline `// cppcheck-suppress` comments, or add
  ids to `.cppcheck-suppressions`. `unusedFunction` in particular tends to fire on
  every public API entry point and usually wants suppressing wholesale.
- **Pin the cppcheck version.** `_cppcheck.yaml` defaults to `ref: main`; set it to
  the tag matching your local cppcheck (2.22 here) so CI and local runs agree on the
  finding set. It's built from source and cached by resolved commit, so a pin costs
  nothing after the first run.
- **`--std=c++23`**, because cppcheck does not understand `c++26` yet, while
  `CMAKE_CXX_STANDARD` is 26. Raise the `std` input when it does.
- **Watch the cache budget.** GitHub gives a repo 10 GB and evicts least-recently-used.
  Seven build trees per commit (3 blocking + 4 toolchains) may thrash it. Check with
  `gh cache list --sort size-in-bytes`; if it thrashes, set `cache-build-tree: false`
  on `build-test-extra`, which is advisory anyway.
- **`lukka/get-cmake@latest` is still an unpinned tag**, carried over as-is. Worth
  pinning to a SHA for the same supply-chain reason everything else is pinned.
- **`/` is not covered by the pre-commit `exclude` regex** (`^\.(vale|github)/`
  doesn't match it), so these files are being linted right now — which is how they
  came out clean. After the rename they fall back under the exclusion.

## Verified

`actionlint` 1.7.12 (with shellcheck 0.11.0 over every embedded `run:` block) and
`yamllint` against `.yamllint.yaml`: clean, warnings only for a few long expression
lines. `restore_mtimes.py` tested against real clones of this repo for both coverage
and determinism.

What is *not* verified: no workflow has actually executed. The first real run is where
runner-specific things — the cppcheck source build, sccache paths on Windows, cache
hit rates — get their first exercise.