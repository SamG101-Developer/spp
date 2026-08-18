#!/usr/bin/env bash
# Recompute the pins this repository records for the code
# it downloads, and rewrite the files holding them.
set -euo pipefail

VERSIONS=".github/versions.env"
LIBS=".github/scripts/setup-toolchain/install-small-libs.sh"
status=0

if ! [ -f "$VERSIONS" ] || ! [ -f "$LIBS" ]; then
  echo "error: run this from the repository root" >&2
  exit 1
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# Authorization helper for the GitHub tokens to get injected
# into commands / requests.
auth=()
if [ -n "${GH_TOKEN:-${GITHUB_TOKEN:-}}" ]; then
  auth=(-H "Authorization: Bearer ${GH_TOKEN:-$GITHUB_TOKEN}")
fi

# General purpose secure fetch command, forcing https and
# retry options. Adds all subsequent text after it.
fetch() { curl --proto '=https' --proto-redir '=https' --tlsv1.2 -fsSL --retry 3 --retry-all-errors "$@"; }

# Read a pinned value straight out of versions.env, the
# same way prune-caches.sh does.
pinned() { grep -oP "^$1=\K.*" "$VERSIONS"; }

# Rewrite a NAME=value line in versions.env. Keys are never
# invented: a name that is not already in the file means the
# file and this script have drifted apart.
set_pin() {
  local key="$1" val="$2" old
  if ! grep -qE "^${key}=" "$VERSIONS"; then
    echo "::error::${key} is not present in ${VERSIONS}"
    status=1
    return 0
  fi
  if [ -z "$val" ]; then
    echo "::error::could not resolve a new value for ${key}; leaving it alone"
    status=1
    return 0
  fi
  old="$(pinned "$key")"
  if [ "$old" = "$val" ]; then
    echo "  ${key} unchanged"
  else
    sed -i -E "s#^${key}=.*#${key}=${val}#" "$VERSIONS"
    echo "  ${key}"
    echo "    ${old}"
    echo " -> ${val}"
  fi
}

# <owner/repo> <tag> <asset-name> -> the asset's sha256,
# or nothing when that tag publishes no such asset. The
# digest is served by the release API.
asset_digest() {
  fetch "${auth[@]}" "https://api.github.com/repos/$1/releases/tags/$2" 2> /dev/null \
    | python3 -c '
import json, sys
want = sys.argv[1]
for a in json.load(sys.stdin).get("assets", []):
    if a["name"] == want:
        print((a.get("digest") or "").removeprefix("sha256:"))
        break
' "$3"
}

# For all the small libs, begin the standardised fresh
# logic.
echo "dependency commits (${LIBS})"
while read -r url; do

  # Try to read from the target repo, to get the newest
  # hash.
  sha="$(git ls-remote "$url" HEAD 2> /dev/null | cut -f 1)"
  name="$(basename "$url" .git)"
  if [ "${#sha}" -ne 40 ]; then
    echo "::error::could not resolve HEAD for ${url}"
    status=1
    continue
  fi

  # Anchor the substitution on the URL, so only the sha
  # on that library's own line moves.
  esc="${url//./\\.}"
  old="$(grep -oE "${esc} [0-9a-f]{40}" "$LIBS" | cut -d ' ' -f 2)"
  if [ "$old" = "$sha" ]; then
    echo "  ${name} unchanged"
  else
    sed -i -E "s#(${esc} )[0-9a-f]{40}#\1${sha}#" "$LIBS"
    echo "  ${name}: ${old} -> ${sha}"
  fi
done < <(grep -oE 'https://github\.com/[^ ]+\.git' "$LIBS" | sort -u)

# Do the google-test-parallel suite manually.
echo
echo "gtest-parallel (${VERSIONS})"
set_pin GTEST_PARALLEL_COMMIT "$(git ls-remote https://github.com/google/gtest-parallel.git HEAD 2> /dev/null | cut -f 1)"

# Do the llvm.sh installer manually.
echo
echo "apt.llvm.org installer (${VERSIONS})"
fetch -o "$tmp/llvm.sh" https://apt.llvm.org/llvm.sh
set_pin LLVM_SH_SHA256 "$(sha256sum "$tmp/llvm.sh" | cut -d ' ' -f 1)"

# The OSV is more complex so has different logic.
echo
echo "osv-scanner (${VERSIONS})"
# Unlike the libraries, this one tracks published releases rather than a branch tip: it is a tool the pipeline runs,
# not a dependency it links, so the useful question is which version is current rather than what landed on main.
osv_tag="$(fetch "${auth[@]}" https://api.github.com/repos/google/osv-scanner/releases/latest \
  | python3 -c 'import json,sys; print(json.load(sys.stdin).get("tag_name",""))')"
if [ -z "$osv_tag" ]; then
  echo "::error::could not read the latest osv-scanner release"
  status=1
else
  set_pin OSV_SCANNER_VERSION "${osv_tag#v}"
  set_pin OSV_SCANNER_SHA256 "$(asset_digest google/osv-scanner "$osv_tag" osv-scanner_linux_amd64)"
fi

# Same shape as osv-scanner: a published release rather than a branch tip, because it is a tool the pipeline runs.
echo
echo "gitleaks (${VERSIONS})"
gitleaks_tag="$(fetch "${auth[@]}" https://api.github.com/repos/gitleaks/gitleaks/releases/latest \
  | python3 -c 'import json,sys; print(json.load(sys.stdin).get("tag_name",""))')"
if [ -z "$gitleaks_tag" ]; then
  echo "::error::could not read the latest gitleaks release"
  status=1
else
  set_pin GITLEAKS_VERSION "${gitleaks_tag#v}"
  set_pin GITLEAKS_SHA256 \
    "$(asset_digest gitleaks/gitleaks "$gitleaks_tag" "gitleaks_${gitleaks_tag#v}_linux_x64.tar.gz")"
  echo "  note: move the gitleaks rev in .pre-commit-config.yaml to ${gitleaks_tag} as well, so the hook and the"
  echo "        CI scan run the same rules"
fi

echo
echo "prebuilt Boost (${VERSIONS})"
boost="$(pinned BOOST_VERSION)"
set_pin BOOST_SHA256_LINUX \
  "$(asset_digest MarkusJx/prebuilt-boost "$boost" "boost-${boost}-ubuntu-24.04-gcc-static+shared-x86.tar.gz")"
# There is no 24.04 arm64 build upstream, and none is needed: only the headers are used, so the 22.04 tarball is what
# the arm64 runners take. Keep this in step with the case block in install-boost.sh.
set_pin BOOST_SHA256_LINUX_ARM64 \
  "$(asset_digest MarkusJx/prebuilt-boost "$boost" "boost-${boost}-ubuntu-22.04-gcc-static+shared-aarch64.tar.gz")"
set_pin BOOST_SHA256_MACOS \
  "$(asset_digest MarkusJx/prebuilt-boost "$boost" "boost-${boost}-macos-15-clang-static+shared-aarch64.tar.gz")"
set_pin BOOST_SHA256_WINDOWS \
  "$(asset_digest MarkusJx/prebuilt-boost "$boost" "boost-${boost}-windows-2025-msvc-static-x86.tar.gz")"

echo
echo "Windows LLVM (checked, not refreshed)"
win_tag="$(pinned LLVM_WIN_TAG)"
win_asset="$(pinned LLVM_WIN_ASSET)"
win_want="$(pinned LLVM_WIN_SHA256)"
win_got="$(asset_digest llvm/llvm-project "$win_tag" "$win_asset")"
if [ -z "$win_got" ]; then
  echo "::error::${win_tag} publishes no asset named ${win_asset}"
  echo "  pick a tag that ships an x86_64 Windows build and move LLVM_WIN_TAG, LLVM_WIN_ASSET and LLVM_WIN_SHA256"
  echo "  together; the asset name is not derivable from the tag, so it has to be read off the release page"
  status=1
elif [ "$win_got" != "$win_want" ]; then
  echo "::error::the asset behind ${win_tag} changed; a published release asset should never do this"
  echo "  pinned ${win_want}"
  echo "  actual ${win_got}"
  status=1
else
  echo "  ${win_asset} matches its pin"
fi

echo
if [ "$status" -eq 0 ]; then
  echo "done; review 'git diff' and commit"
else
  echo "finished with errors; see above" >&2
fi
exit "$status"
