#!/usr/bin/env bash
# Create the GitHub release for this version and attach one
# binary per platform. The tag is created here rather than
# before the builds, so a matrix that goes red leaves no tag
# behind and the next push retries the same version cleanly.
set -euo pipefail

CHANGELOG_DIR="${CHANGELOG_DIR:-changelog}"
DIST_DIR="${DIST_DIR:-dist}"
notes="${CHANGELOG_DIR}/${VERSION}.md"

[ -f "$notes" ] || { echo "::error::${notes} is missing"; exit 1; }

# The matrix names one asset per platform; an empty directory
# means every upload silently produced nothing, which would
# otherwise publish an assetless release.
mapfile -t assets < <(find "$DIST_DIR" -type f | sort)
[ "${#assets[@]}" -gt 0 ] || { echo "::error::no binaries found under ${DIST_DIR}"; exit 1; }

printf 'attaching %d asset(s):\n' "${#assets[@]}"
printf '  %s\n' "${assets[@]}"

gh release create "v${VERSION}" \
  --target "$GITHUB_SHA" \
  --title "S++ ${VERSION}" \
  --notes-file "$notes" \
  "${assets[@]}"

{
  echo "### Released [S++ ${VERSION}](${GITHUB_SERVER_URL}/${GITHUB_REPOSITORY}/releases/tag/v${VERSION})"
  echo
  # shellcheck disable=SC2016  # the backticks are markdown for the summary, not a substitution
  printf -- '- `%s`\n' "${assets[@]##*/}"
} >> "${GITHUB_STEP_SUMMARY:-/dev/null}"
