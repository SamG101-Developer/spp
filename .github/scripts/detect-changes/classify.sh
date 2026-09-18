#!/usr/bin/env bash
# Decide, per gated job, whether this diff can move that job's result.
#
# Most gates read "the code changed, or a CI path that job reads changed", and none of those fire for a diff that is
# nothing but pinned `uses:` bumps: dependabot rewrites one action's sha in every workflow, pr.yaml included, and
# nothing about that can change a build. Security is the exception - the pins are what it has to look at. An
# unreachable base means "assume everything changed", which is the one direction this must never fail in.
set -euo pipefail
set -f  # the globs below are patterns to match against, never paths to expand

# Paths that decide each gate, as shell globs, whitespace-separated. `code` is stated by exclusion: everything that
# is not documentation, editor state or CI, plus the few CI paths that feed the build itself.
CODE_SKIP='docs/* .vale/* .idea/* *.md .gitignore pyproject.toml uv.lock VERSION changelog/* .github/*'
CODE_ALSO='.github/dependencies.toml .github/scripts/lib/* .github/actions/detect-changes/* .github/scripts/detect-changes/*'

# Everything that decides how the project is configured and compiled, which every build-shaped job reads.
TOOLCHAIN='.github/actions/setup-python/* .github/actions/setup-toolchain/* .github/actions/build-cache-key/*
  .github/actions/configure-cmake/* .github/scripts/setup-toolchain/* .github/scripts/build-cache-key/*
  .github/scripts/configure-cmake/*'

RELEASE_GATE='VERSION changelog/* .github/workflows/pr.yaml .github/scripts/version-check/*'
CPPCHECK='.github/workflows/pr.yaml .github/workflows/_cppcheck.yaml .github/actions/setup-python/*
  .github/scripts/lint/*'
COMPILE=".github/workflows/pr.yaml .github/workflows/post_merge.yaml .github/workflows/_build.yaml
  .github/actions/compile/* .github/actions/restore-mtimes/* .github/actions/run-tests/*
  .github/scripts/run-tests/* .github/scripts/stl-conformance/* ${TOOLCHAIN}"
CROSS_CODEGEN=".github/workflows/pr.yaml .github/workflows/_cross_codegen.yaml .github/actions/restore-mtimes/*
  .github/scripts/cross-codegen/* ${TOOLCHAIN}"
SECURITY=".github/workflows/pr.yaml .github/workflows/_security.yaml .github/scripts/security/* ${TOOLCHAIN}"
SONAR=".github/workflows/pr.yaml .github/workflows/post_merge.yaml .github/workflows/_sonar.yaml
  .github/actions/restore-mtimes/* ${TOOLCHAIN}"

# A pinned `uses:`, as a step key or a list item, with the version comment dependabot keeps beside it.
PIN_RE='^[+-][[:space:]]*(-[[:space:]]+)?uses:[[:space:]]*[^[:space:]]+@[0-9a-fA-F]{40}([[:space:]]+#.*)?$'

emit() {
  echo "$1=$2" >> "${GITHUB_OUTPUT:-/dev/stdout}"
  printf '  %-14s %s\n' "$1" "$2"
}

if [ -z "${BASE_SHA:-}" ] \
  || [ "$BASE_SHA" = "0000000000000000000000000000000000000000" ] \
  || ! git cat-file -e "${BASE_SHA}^{commit}" 2> /dev/null; then
  echo "no usable base commit; assuming everything changed"
  emit ci_pins_only false
  for gate in code release_gate version cppcheck compile cross_codegen security sonar; do
    emit "$gate" true
  done
  exit 0
fi

mapfile -t changed < <(git diff --name-only "$BASE_SHA" "$HEAD_SHA")
printf 'changed %d file(s)\n' "${#changed[@]}"

# True when any changed file matches one of the globs in $1.
any() {
  local file pattern
  for file in "${changed[@]}"; do
    for pattern in $1; do
      # shellcheck disable=SC2254  # the glob is the pattern
      case "$file" in $pattern) return 0 ;; esac
    done
  done
  return 1
}

# True when any changed file matches none of the globs in $1.
any_outside() {
  local file pattern skip
  for file in "${changed[@]}"; do
    skip=false
    for pattern in $1; do
      # shellcheck disable=SC2254
      case "$file" in $pattern) skip=true; break ;; esac
    done
    [ "$skip" = true ] || return 0
  done
  return 1
}

code=false
if any_outside "$CODE_SKIP" || any "$CODE_ALSO"; then
  code=true
fi

# A diff of nothing but pin bumps gates nothing, whichever files it touched. A pin lives in a workflow or a composite
# action and nowhere else, so anything outside those settles it without reading the diff.
pins_only=false
if [ "${#changed[@]}" -gt 0 ] && ! any_outside '.github/workflows/*.yaml .github/workflows/*.yml .github/actions/*'; then
  pins_only=true
  # -U0 drops the context lines, leaving the file headers - which start with the same +/- as a real change - and the
  # changes themselves, every one of which has to be a pin.
  while IFS= read -r line; do
    case "$line" in
      '+++ '* | '--- '*) continue ;;
      '+'* | '-'*) ;;
      *) continue ;;
    esac
    if ! [[ "$line" =~ $PIN_RE ]]; then
      echo "not a pin bump: ${line}"
      pins_only=false
      break
    fi
  done < <(git diff -U0 "$BASE_SHA" "$HEAD_SHA" -- "${changed[@]}")
fi

# `pinnable` marks the gates a pure pin bump still has to run.
gate() {
  local name="$1" paths="$2" pinnable="${3:-no}"
  if [ "$pins_only" = true ] && [ "$pinnable" = no ]; then
    emit "$name" false
  elif [ "$code" = true ] || any "$paths"; then
    emit "$name" true
  else
    emit "$name" false
  fi
}

emit code "$code"
emit ci_pins_only "$pins_only"
gate release_gate "$RELEASE_GATE"
gate cppcheck "$CPPCHECK"
gate compile "$COMPILE"
gate cross_codegen "$CROSS_CODEGEN"
gate security "$SECURITY" pins
gate sonar "$SONAR"

# Not a "the code moved" gate: only a VERSION bump is a release candidate.
if any VERSION; then emit version true; else emit version false; fi
