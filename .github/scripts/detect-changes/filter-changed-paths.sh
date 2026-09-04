#!/usr/bin/env bash
# Decide, per gated CI job, whether this ref changed anything
# that can affect that job's build, test or analysis result,
# and report each one as its own step output.
set -euo pipefail

# The gated jobs, in output order. `code` is the coarse
# "source, build system or test data moved" signal; each
# target below is `code` widened by the CI files that only
# that one job reads, so an action-pin bump re-runs the job
# it touches and nothing else.
targets=(cppcheck compile cross_codegen security sonar)

# Each pattern is the CI files in that job's dependency
# closure: its reusable workflow, the composite actions that
# workflow calls, and the caller that passes it its inputs. A
# CI file matching none of them (dependabot.yaml, claude.yaml,
# pr_title.yaml, the docs and lint workflows) gates nothing.
declare -A patterns=(
  [cppcheck]='^\.github/(workflows/(pr|_cppcheck)\.yaml$|actions/setup-python/)'
  [compile]='^\.github/(workflows/(pr|post_merge|_compile)\.yaml$|actions/(restore-mtimes|setup-toolchain|build-cache-key|configure-cmake|run-tests)/)'
  [cross_codegen]='^\.github/(workflows/(pr|_cross_codegen)\.yaml$|actions/(restore-mtimes|setup-python|setup-toolchain|build-cache-key|configure-cmake)/)'
  [security]='^\.github/(workflows/(pr|_security)\.yaml$|actions/(setup-python|setup-toolchain|configure-cmake)/)'
  [sonar]='^\.github/(workflows/(pr|post_merge|_sonar)\.yaml$|actions/(restore-mtimes|setup-python|setup-toolchain|build-cache-key|configure-cmake)/)'
)

# The inert paths change nothing any job reads; they are
# generally documentation or editor configuration.
inert='^(docs/|\.vale/|\.idea/|[^/]*\.md$|\.gitignore$)'

# The CI paths can move a job's behaviour, but only for the
# jobs that read them, so they are routed by the patterns
# above instead of setting `code`.
ci='^\.github/'

# detect-changes is the gate itself, so it is held out of the
# CI class: a change to it must re-run everything it could
# have wrongly skipped.
gate='^\.github/(actions|scripts)/detect-changes/'

declare -A hit
for t in "${targets[@]}"; do
  hit[$t]=false
done

# Write `code` and every target to the step output, each
# target ORed against `code`.
emit() {
  local code=$1 t value
  echo "code=${code}" | tee -a "$GITHUB_OUTPUT"
  for t in "${targets[@]}"; do
    value=false
    if [ "$code" = true ] || [ "${hit[$t]}" = true ]; then
      value=true
    fi
    echo "${t}=${value}" | tee -a "$GITHUB_OUTPUT"
  done
}

# No usable base (manual dispatch, merge queue, new branch,
# force push to an unknown commit): fall back to building,
# because we cannot prove it is safe to skip.
if [ -z "$BASE_SHA" ] \
  || [ "$BASE_SHA" = "0000000000000000000000000000000000000000" ] \
  || ! git cat-file -e "${BASE_SHA}^{commit}" 2>/dev/null; then
  echo "no usable base commit; assuming code changed"
  emit true
  exit 0
fi

# Detect changed files between the base and head commits, and
# decide from them which jobs could have moved.
files=$(git diff --name-only "$BASE_SHA" "$HEAD_SHA")
echo "Changed files:"
echo "$files"

code=false
while IFS= read -r f; do
  [ -n "$f" ] || continue

  # Inert: reads on no path, so skip it entirely.
  if grep -qE "$inert" <<< "$f"; then
    continue
  fi

  # CI-only: widen just the jobs whose closure it is in.
  if grep -qE "$ci" <<< "$f" && ! grep -qE "$gate" <<< "$f"; then
    for t in "${targets[@]}"; do
      if grep -qE "${patterns[$t]}" <<< "$f"; then
        hit[$t]=true
      fi
    done
    continue
  fi

  # Anything else is source, build system or test data.
  code=true
done <<< "$files"

emit "$code"
