#!/usr/bin/env bash
# Recompute the pins this repository records for the code
# it downloads, and rewrite the files holding them.
set -euo pipefail

MANIFEST=".github/dependencies.toml"
PINS=".github/scripts/lib/pins.py"
status=0

if ! [ -f "$MANIFEST" ] || ! [ -f "$PINS" ]; then
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

# Everything below addresses the manifest by dotted path, and both directions go through pins.py, so the rules about
# what a pin may contain are enforced in one place rather than restated here as a regex.
pinned() { python3 "$PINS" get "$1"; }

# Rewrite one value in the manifest. Paths are never invented: pins.py fails on a path that is not already there,
# which is what catches this script and the manifest drifting apart.
set_pin() {
  local path="$1" val="$2" old
  if [ -z "$val" ]; then
    echo "::error::could not resolve a new value for ${path}; leaving it alone"
    status=1
    return 0
  fi
  old="$(pinned "$path" 2> /dev/null || true)"
  if [ "$old" = "$val" ]; then
    echo "  ${path} unchanged"
    return 0
  fi
  if ! python3 "$PINS" set "$path" "$val"; then
    status=1
    return 0
  fi
  echo "  ${path}"
  echo "    ${old}"
  echo " -> ${val}"
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

# For all the small libs, begin the standardised fresh logic. The list comes from the manifest, so a library added
# there is picked up here without touching this script.
echo "dependency commits (${MANIFEST})"
while IFS=$'\t' read -r -u 3 name repo _commit _flags; do

  # Try to read from the target repo, to get the newest
  # hash.
  sha="$(git ls-remote "$repo" HEAD 2> /dev/null | cut -f 1)"
  if [ "${#sha}" -ne 40 ]; then
    echo "::error::could not resolve HEAD for ${repo}"
    status=1
    continue
  fi
  set_pin "library.${name}.commit" "$sha"
done 3< <(python3 "$PINS" libraries)

# Do the google-test-parallel suite manually.
echo
echo "gtest-parallel (${MANIFEST})"
set_pin pin.gtest-parallel.commit "$(git ls-remote https://github.com/google/gtest-parallel.git HEAD 2> /dev/null | cut -f 1)"

# Do the llvm.sh installer manually.
echo
echo "apt.llvm.org installer (${MANIFEST})"
fetch -o "$tmp/llvm.sh" https://apt.llvm.org/llvm.sh
set_pin pin.llvm-sh.sha256 "$(sha256sum "$tmp/llvm.sh" | cut -d ' ' -f 1)"

# The OSV is more complex so has different logic.
echo
echo "osv-scanner (${MANIFEST})"
# Unlike the libraries, this one tracks published releases rather than a branch tip: it is a tool the pipeline runs,
# not a dependency it links, so the useful question is which version is current rather than what landed on main.
osv_tag="$(fetch "${auth[@]}" https://api.github.com/repos/google/osv-scanner/releases/latest \
  | python3 -c 'import json,sys; print(json.load(sys.stdin).get("tag_name",""))')"
if [ -z "$osv_tag" ]; then
  echo "::error::could not read the latest osv-scanner release"
  status=1
else
  set_pin pin.osv-scanner.version "${osv_tag#v}"
  set_pin pin.osv-scanner.sha256 "$(asset_digest google/osv-scanner "$osv_tag" osv-scanner_linux_amd64)"
fi

# Same shape as osv-scanner: a published release rather than a branch tip, because it is a tool the pipeline runs.
echo
echo "gitleaks (${MANIFEST})"
gitleaks_tag="$(fetch "${auth[@]}" https://api.github.com/repos/gitleaks/gitleaks/releases/latest \
  | python3 -c 'import json,sys; print(json.load(sys.stdin).get("tag_name",""))')"
if [ -z "$gitleaks_tag" ]; then
  echo "::error::could not read the latest gitleaks release"
  status=1
else
  set_pin pin.gitleaks.version "${gitleaks_tag#v}"
  set_pin pin.gitleaks.sha256 \
    "$(asset_digest gitleaks/gitleaks "$gitleaks_tag" "gitleaks_${gitleaks_tag#v}_linux_x64.tar.gz")"
  echo "  note: move the gitleaks rev in .pre-commit-config.yaml to ${gitleaks_tag} as well, so the hook and the"
  echo "        CI scan run the same rules"
fi

# Tracks release tags rather than the branch tip: it is a tool the pipeline runs, and a dev build of an analyser that
# gates every merge is not what this wants. The repository redirects from danmar/cppcheck, so it is addressed by id.
echo
echo "cppcheck (${MANIFEST})"
cppcheck_tag="$(fetch "${auth[@]}" 'https://api.github.com/repositories/143131/tags?per_page=1' \
  | python3 -c 'import json,sys; t=json.load(sys.stdin); print(t[0]["name"] if t else "")')"
if [ -z "$cppcheck_tag" ]; then
  echo "::error::could not read the latest cppcheck tag"
  status=1
else
  set_pin pin.cppcheck.version "$cppcheck_tag"
  set_pin pin.cppcheck.commit "$(fetch "${auth[@]}" "https://api.github.com/repositories/143131/git/ref/tags/${cppcheck_tag}" \
    | python3 -c 'import json,sys; print(json.load(sys.stdin)["object"]["sha"])')"
fi

echo
echo "prebuilt Boost (${MANIFEST})"
boost="$(pinned pin.boost.version)"
set_pin pin.boost.sha256-linux \
  "$(asset_digest MarkusJx/prebuilt-boost "$boost" "boost-${boost}-ubuntu-24.04-gcc-static+shared-x86.tar.gz")"
# There is no 24.04 arm64 build upstream, and none is needed: only the headers are used, so the 22.04 tarball is what
# the arm64 runners take. Keep this in step with the case block in install-boost.sh.
set_pin pin.boost.sha256-linux-arm64 \
  "$(asset_digest MarkusJx/prebuilt-boost "$boost" "boost-${boost}-ubuntu-22.04-gcc-static+shared-aarch64.tar.gz")"
set_pin pin.boost.sha256-macos \
  "$(asset_digest MarkusJx/prebuilt-boost "$boost" "boost-${boost}-macos-15-clang-static+shared-aarch64.tar.gz")"
set_pin pin.boost.sha256-windows \
  "$(asset_digest MarkusJx/prebuilt-boost "$boost" "boost-${boost}-windows-2025-msvc-static-x86.tar.gz")"

echo
echo "Windows LLVM (checked, not refreshed)"
win_tag="$(pinned pin.llvm-win.tag)"
win_asset="$(pinned pin.llvm-win.asset)"
win_want="$(pinned pin.llvm-win.sha256)"
win_got="$(asset_digest llvm/llvm-project "$win_tag" "$win_asset")"
if [ -z "$win_got" ]; then
  echo "::error::${win_tag} publishes no asset named ${win_asset}"
  echo "  pick a tag that ships an x86_64 Windows build and move tag, asset and sha256 under [pin.llvm-win] together;"
  echo "  the asset name is not derivable from the tag, so it has to be read off the release page"
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
