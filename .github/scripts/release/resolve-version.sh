#!/usr/bin/env bash
# Decide whether this master commit is a release, and publish
# the answer as step outputs. The tag is the record of what has
# been published, not the diff: a re-run of the same push, or a
# merge that changed no version, both land on an existing tag
# and must not build a second set of binaries for it.
set -euo pipefail

VERSION_FILE="${VERSION_FILE:-VERSION}"
CHANGELOG_DIR="${CHANGELOG_DIR:-changelog}"
SEMVER_RE='^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$'

emit() {
  {
    echo "release=$1"
    echo "version=${2:-}"
  } >> "${GITHUB_OUTPUT:-/dev/null}"
  echo "$3"
  exit 0
}

[ -f "$VERSION_FILE" ] || emit false "" "no ${VERSION_FILE}; nothing to release"
version="$(tr -d '[:space:]' < "$VERSION_FILE")"

# The PR gate rejects both of these, so reaching them means a
# direct push to master. Skip rather than fail: a red release
# job on master is noise nobody can fix by re-running it.
[[ "$version" =~ $SEMVER_RE ]] \
  || emit false "" "${VERSION_FILE} contains '${version}', which is not a version; not releasing"
[ -f "${CHANGELOG_DIR}/${version}.md" ] \
  || emit false "" "${CHANGELOG_DIR}/${version}.md is missing; not releasing ${version}"

if gh release view "v${version}" >/dev/null 2>&1; then
  emit false "$version" "v${version} is already released"
fi

emit true "$version" "releasing S++ ${version}"
