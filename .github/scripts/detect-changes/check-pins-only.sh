#!/usr/bin/env bash
# Report whether this diff is nothing but action pin bumps.
#
# A grouped dependabot github-actions PR rewrites one action's
# pinned SHA everywhere it appears, and harden-runner and
# checkout appear in every workflow file there is - pr.yaml
# included. pr.yaml is listed in every path filter the caller
# owns, so without this the cheapest change in the repository
# lights up the entire matrix and demands a release bump with
# it. Nothing about such a diff can move a build: no source, no
# script and no job definition changed, and the pins have
# already sat out their cooldown.
set -euo pipefail

# A pinned `uses:`, as a step key or a list item, with the
# version comment dependabot keeps beside it.
PIN_RE='^[+-][[:space:]]*(-[[:space:]]+)?uses:[[:space:]]*[^[:space:]]+@[0-9a-fA-F]{40}([[:space:]]+#.*)?$'

emit() {
  echo "pins_only=$1" >> "${GITHUB_OUTPUT:-/dev/null}"
  echo "$2"
  exit 0
}

mapfile -t changed < <(git diff --name-only "$BASE_SHA" "$HEAD_SHA")
[ "${#changed[@]}" -gt 0 ] || emit false "no changed files to classify"

# A pin lives in a workflow or a composite action and nowhere
# else, so anything outside those settles it without a diff.
for file in "${changed[@]}"; do
  case "$file" in
  .github/workflows/*.yaml | .github/workflows/*.yml | .github/actions/*) ;;
  *) emit false "${file} is not a workflow definition" ;;
  esac
done

# -U0 drops the context lines, leaving the file headers - which
# start with the same +/- as a real change - and the changes
# themselves, every one of which has to be a pin.
while IFS= read -r line; do
  case "$line" in
  '+++ '* | '--- '*) continue ;;
  '+'* | '-'*) ;;
  *) continue ;;
  esac
  [[ "$line" =~ $PIN_RE ]] || emit false "not a pin bump: ${line}"
done < <(git diff -U0 "$BASE_SHA" "$HEAD_SHA" -- "${changed[@]}")

emit true "every change is an action pin bump; gating nothing"
