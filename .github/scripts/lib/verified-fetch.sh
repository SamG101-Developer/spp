# Checksum-verified download helper, shared by every step
# that pulls an artefact off the network. After a deliberate
# version bump the recorded digests will no longer match.
# Run .github/scripts/security/refresh-pins.sh to recompute
# them, then commit the result.
# shellcheck shell=bash

# Print the sha256 of a file. Ubuntu and the Git-for-Windows
# coreutils have sha256sum; macOS ships shasum instead and
# has no sha256sum at all, and install-boost.sh runs on all
# three.
_sha256_of() {
  if command -v sha256sum > /dev/null 2>&1; then
    sha256sum "$1" | cut -d ' ' -f 1
  else
    shasum -a 256 "$1" | cut -d ' ' -f 1
  fi
}

# verified_fetch <url> <destination> <expected-sha256>
verified_fetch() {
  local url="$1" dest="$2" want="$3" got

  if [ -z "$want" ]; then
    echo "::error::no sha256 is pinned for ${url}; add one to .github/versions.env"
    return 1
  fi

  curl --proto '=https' --proto-redir '=https' --tlsv1.2 \
    --fail --silent --show-error --location \
    --retry 3 --retry-all-errors \
    --output "$dest" "$url"

  # Verify the download against the pinned digest. A mismatch is
  # a security failure, so delete the file to avoid leaving
  # malformed or malicious content behind after the failure.
  got="$(_sha256_of "$dest")"
  if [ "$got" != "$want" ]; then
    rm -f "$dest"
    echo "::error::sha256 mismatch for ${url}"
    echo "  expected ${want}"
    echo "  actual   ${got}"
    return 1
  fi

  echo "verified ${dest##*/} (sha256 ${got})"
}
