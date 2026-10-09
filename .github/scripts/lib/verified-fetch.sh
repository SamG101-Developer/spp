# Checksum-verified download, shared by every step that pulls an
# artefact off the network. After a version bump the digests no
# longer match: run .github/scripts/maint/refresh-pins.sh.
# shellcheck shell=bash

# macOS ships shasum and no sha256sum. Also used on its own, to
# hand a built binary's digest from one job to the next.
sha256_of() {
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
    echo "::error::no sha256 is pinned for ${url}; add one to .github/dependencies.toml"
    return 1
  fi

  curl --proto '=https' --proto-redir '=https' --tlsv1.2 \
    --fail --silent --show-error --location \
    --retry 3 --retry-all-errors \
    --output "$dest" "$url"

  # A mismatch is a security failure, so the file goes rather than
  # being left behind.
  got="$(sha256_of "$dest")"
  if [ "$got" != "$want" ]; then
    rm -f "$dest"
    echo "::error::sha256 mismatch for ${url}"
    echo "  expected ${want}"
    echo "  actual   ${got}"
    return 1
  fi

  echo "verified ${dest##*/} (sha256 ${got})"
}

# verified_install_bin <url> <sha256> <name> [tar-member]: fetch, verify and put <name> on PATH.
verified_install_bin() {
  local url="$1" want="$2" name="$3" member="${4:-}"
  local bin="$HOME/.tools/bin" download="${RUNNER_TEMP}/${name}.download"

  mkdir -p "$bin"
  verified_fetch "$url" "$download" "$want"
  if [ -n "$member" ]; then
    tar -xzf "$download" -C "$bin" "$member"
    [ "$member" = "$name" ] || mv "${bin}/${member}" "${bin}/${name}"
  else
    mv "$download" "${bin}/${name}"
  fi
  chmod +x "${bin}/${name}"
  echo "$bin" >> "$GITHUB_PATH"
}
