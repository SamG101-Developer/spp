#!/usr/bin/env bash
# Gate a merge on the two things a release needs and nothing
# else can supply after the fact: a version number nobody has
# published yet, and the changelog entry that becomes that
# release's notes.
set -euo pipefail

VERSION_FILE="${VERSION_FILE:-VERSION}"
CHANGELOG_DIR="${CHANGELOG_DIR:-changelog}"
SEMVER_RE='^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$'

fail() {
  echo "::error::$1"
  exit 1
}

# The base for a pull_request event, or a merge_group's, and
# otherwise the default branch: a workflow_dispatch has neither
# payload, and comparing against the branch is what it means.
base="${BASE_SHA:-}"
if [ -z "$base" ] || ! git cat-file -e "${base}^{commit}" 2>/dev/null; then
  base="origin/${DEFAULT_BRANCH:-master}"
  git cat-file -e "${base}^{commit}" 2>/dev/null \
    || fail "no usable base commit to compare ${VERSION_FILE} against"
fi

# A manual dispatch from the default branch resolves the base to
# the commit it is already on. There is no bump to look for in a
# diff against itself.
if [ "$(git rev-parse "${base}^{commit}")" = "$(git rev-parse 'HEAD^{commit}')" ]; then
  echo "::notice::head is the base commit; nothing to compare"
  exit 0
fi

[ -f "$VERSION_FILE" ] || fail "${VERSION_FILE} is missing from this branch"
head_version="$(tr -d '[:space:]' < "$VERSION_FILE")"

# A base that predates the VERSION file has no version at all,
# which any valid bump is ahead of.
base_version="$(git show "${base}:${VERSION_FILE}" 2>/dev/null | tr -d '[:space:]' || true)"
base_version="${base_version:-0.0.0}"

[[ "$head_version" =~ $SEMVER_RE ]] \
  || fail "${VERSION_FILE} contains '${head_version}', which is not a MAJOR.MINOR.PATCH version"

if [ "$head_version" = "$base_version" ]; then
  fail "${VERSION_FILE} is still ${head_version}; bump it and add ${CHANGELOG_DIR}/<new version>.md"
fi

# sort -V puts the older version first, so the head version is
# ahead exactly when it is the one that sorts last.
newest="$(printf '%s\n%s\n' "$head_version" "$base_version" | sort -V | tail -1)"
[ "$newest" = "$head_version" ] \
  || fail "${VERSION_FILE} moved backwards, from ${base_version} to ${head_version}"

# A version already carrying a tag was published from another
# branch while this one was open; its release exists and cannot
# be rewritten, so the bump has to go further.
if git rev-parse -q --verify "refs/tags/v${head_version}" >/dev/null; then
  fail "v${head_version} is already released; bump ${VERSION_FILE} past it"
fi

entry="${CHANGELOG_DIR}/${head_version}.md"
[ -f "$entry" ] || fail "${entry} is missing; it becomes the body of the S++ ${head_version} release"
[ -s "$entry" ] || fail "${entry} is empty; it becomes the body of the S++ ${head_version} release"

echo "version=${head_version}" >> "${GITHUB_OUTPUT:-/dev/null}"
echo "${base_version} -> ${head_version}, notes in ${entry}"

{
  echo "### Release gate"
  echo
  echo "\`${VERSION_FILE}\`: \`${base_version}\` → \`${head_version}\`"
  echo
  echo "Notes for **S++ ${head_version}**, from \`${entry}\`:"
  echo
  echo '```markdown'
  cat "$entry"
  echo '```'
} >> "${GITHUB_STEP_SUMMARY:-/dev/null}"
