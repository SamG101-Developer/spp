#!/usr/bin/env bash
# Assemble the release from the tested lanes. `add <label> <exe-suffix>` verifies one lane's binaries and copies its
# spp into dist/; `seal` writes dist/SHA256SUMS and the `checksums` output.
set -euo pipefail
source .github/scripts/lib/version.sh

is_semver "${VERSION:-}" || { echo "::error::bad version '${VERSION:-}'"; exit 1; }

case "${1:-}" in
  add)
    label="$2"
    exe="$3"
    [[ "$label" =~ ^[a-z0-9]+(-[a-z0-9]+)*$ ]] || { echo "::error::bad label '${label}'"; exit 1; }
    case "$exe" in "" | .exe) ;; *) echo "::error::bad exe suffix '${exe}'"; exit 1 ;; esac

    BIN_DIR="bins/${label}" BIN_EXE="$exe" bash .github/scripts/lib/verify-binaries.sh
    mkdir -p dist
    cp "bins/${label}/spp${exe}" "dist/spp-${VERSION}-${label}${exe}"
    echo "collected spp-${VERSION}-${label}${exe}"
    ;;

  seal)
    [ -e dist/SHA256SUMS ] && { echo "::error::dist/SHA256SUMS already exists"; exit 1; }
    (cd dist && sha256sum -- spp-* > SHA256SUMS)
    cat dist/SHA256SUMS

    # A random delimiter, so the content cannot end the output early.
    delim="EOF_$(od -An -N16 -tx1 /dev/urandom | tr -d ' \n')"
    {
      echo "checksums<<${delim}"
      cat dist/SHA256SUMS
      echo "$delim"
    } >> "$GITHUB_OUTPUT"
    ;;

  *)
    echo "::error::usage: collect.sh add <label> <exe-suffix> | collect.sh seal"
    exit 1
    ;;
esac
