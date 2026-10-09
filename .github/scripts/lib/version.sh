# Where the release version and its notes live, and what a version looks like.
# shellcheck shell=bash

VERSION_FILE="${VERSION_FILE:-VERSION}"
CHANGELOG_DIR="${CHANGELOG_DIR:-changelog}"

# is_semver <text>: MAJOR.MINOR.PATCH, no leading zeros, nothing else.
is_semver() {
  [[ "$1" =~ ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$ ]]
}

# read_version [file]: the file's version with its whitespace gone.
read_version() {
  tr -d '[:space:]' < "${1:-$VERSION_FILE}"
}
