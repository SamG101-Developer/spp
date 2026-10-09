#!/usr/bin/env bash
# Create the release with one binary per platform and the attested SHA256SUMS. The tag is created
# here, so a red lane leaves no tag behind and the next push retries cleanly.
set -euo pipefail
source .github/scripts/lib/version.sh

DIST_DIR="${DIST_DIR:-dist}"
notes="${CHANGELOG_DIR}/${VERSION}.md"

[ -f "$notes" ] || { echo "::error::${notes} is missing"; exit 1; }

# An empty directory would otherwise publish an assetless release.
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
  echo
  # shellcheck disable=SC2016
  printf 'Verify a download with `gh attestation verify <file> --repo %s`.\n' "$GITHUB_REPOSITORY"
} >> "${GITHUB_STEP_SUMMARY:-/dev/null}"
