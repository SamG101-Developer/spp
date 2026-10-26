#!/usr/bin/env bash
# Recompute the pins this repository records for the code it
# downloads, and rewrite the files holding them.
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

# Authorization helper for the GitHub tokens to get injected into
# commands / requests.
auth=()
if [ -n "${GH_TOKEN:-${GITHUB_TOKEN:-}}" ]; then
  auth=(-H "Authorization: Bearer ${GH_TOKEN:-$GITHUB_TOKEN}")
fi

# General purpose secure fetch command, forcing https and retry
# options. Adds all subsequent text after it.
fetch() { curl --proto '=https' --proto-redir '=https' --tlsv1.2 -fsSL --retry 3 --retry-all-errors "$@"; }

# Everything below addresses the manifest by dotted path,
# and both directions go through pins.py, so the rules
# about what a pin may contain are enforced in one place
# rather than restated here as a regex.
pinned() { python3 "$PINS" get "$1"; }

# Rewrite one value in the manifest. Paths are never invented:
# pins.py fails on a path that is not already there, which is
# what catches this script and the manifest drifting apart.
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

# <owner/repo> <tag> <asset-name> -> the asset's sha256, or
# nothing when that tag publishes no such asset. The digest
# is served by the release API.
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

# For all the small libs, begin the standardised fresh logic.
# The list comes from the manifest, so a library added there
# is picked up here without touching this script.
echo "dependency commits (${MANIFEST})"
while IFS=$'\t' read -r -u 3 name repo _commit _flags; do

  # Try to read from the target repo, to get the newest hash.
  sha="$(git ls-remote "$repo" HEAD 2> /dev/null | cut -f 1)"
  if [ "${#sha}" -ne 40 ]; then
    echo "::error::could not resolve HEAD for ${repo}"
    status=1
    continue
  fi
  set_pin "library.${name}.commit" "$sha"
done 3< <(python3 "$PINS" libraries)

# GOOGLE-TEST-PARALLEL
echo
echo "gtest-parallel (${MANIFEST})"
set_pin pin.gtest-parallel.commit "$(git ls-remote https://github.com/google/gtest-parallel.git HEAD 2> /dev/null | cut -f 1)"

# LLVM.SH
echo
echo "apt.llvm.org installer (${MANIFEST})"
fetch -o "$tmp/llvm.sh" https://apt.llvm.org/llvm.sh
set_pin pin.llvm-sh.sha256 "$(sha256sum "$tmp/llvm.sh" | cut -d ' ' -f 1)"

# <owner/repo> -> the tag of its latest release, or nothing.
latest_tag() {
  fetch "${auth[@]}" "https://api.github.com/repos/$1/releases/latest" 2> /dev/null \
    | python3 -c 'import json,sys; print(json.load(sys.stdin).get("tag_name",""))'
}

# OSV SCANNER
echo
echo "osv-scanner (${MANIFEST})"
osv_tag="$(latest_tag google/osv-scanner)"
if [ -z "$osv_tag" ]; then
  echo "::error::could not read the latest osv-scanner release"
  status=1
else
  set_pin pin.osv-scanner.version "${osv_tag#v}"
  set_pin pin.osv-scanner.sha256 "$(asset_digest google/osv-scanner "$osv_tag" osv-scanner_linux_amd64)"
fi

# GITLEAKS
echo
echo "gitleaks (${MANIFEST})"
gitleaks_tag="$(latest_tag gitleaks/gitleaks)"
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

# CCACHE
echo
echo "ccache (${MANIFEST})"
ccache_tag="$(latest_tag ccache/ccache)"
if [ -z "$ccache_tag" ]; then
  echo "::error::could not read the latest ccache release"
  status=1
else
  set_pin pin.ccache.version "${ccache_tag#v}"
  for platform in linux-x86_64-musl-static:linux-x86-64 linux-aarch64-musl-static:linux-aarch64 darwin:darwin; do
    set_pin "pin.ccache.sha256-${platform#*:}" \
      "$(asset_digest ccache/ccache "$ccache_tag" "ccache-${ccache_tag#v}-${platform%%:*}.tar.gz")"
  done
fi

# SCCACHE
echo
echo "sccache (${MANIFEST})"
sccache_tag="$(latest_tag mozilla/sccache)"
if [ -z "$sccache_tag" ]; then
  echo "::error::could not read the latest sccache release"
  status=1
else
  set_pin pin.sccache.version "${sccache_tag#v}"
  set_pin pin.sccache.sha256 \
    "$(asset_digest mozilla/sccache "$sccache_tag" "sccache-${sccache_tag}-x86_64-pc-windows-msvc.tar.gz")"
fi

# CPPCHECK
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

# VALGRIND
echo
echo "valgrind (${MANIFEST})"
valgrind_version="$(fetch https://sourceware.org/pub/valgrind/ \
  | grep -oE 'valgrind-[0-9]+\.[0-9]+\.[0-9]+\.tar\.bz2' | sed -E 's/^valgrind-|\.tar\.bz2$//g' | sort -uV | tail -1)"
if [ -z "$valgrind_version" ]; then
  echo "::error::could not read the latest valgrind release"
  status=1
else
  fetch -o "$tmp/valgrind.tar.bz2" "https://sourceware.org/pub/valgrind/valgrind-${valgrind_version}.tar.bz2"
  set_pin pin.valgrind.version "$valgrind_version"
  set_pin pin.valgrind.sha256 "$(sha256sum "$tmp/valgrind.tar.bz2" | cut -d ' ' -f 1)"
fi

# QEMU
echo
echo "qemu (${MANIFEST})"
qemu_pool="https://deb.debian.org/debian/pool/main/q/qemu"
qemu_version="$(fetch "${qemu_pool}/" \
  | grep -oE 'qemu-user_[0-9][^_"]*_amd64\.deb' | sed -E 's/^qemu-user_|_amd64\.deb$//g' | sort -uV | tail -1)"
if [ -z "$qemu_version" ]; then
  echo "::error::could not read the latest qemu-user package"
  status=1
else
  fetch -o "$tmp/qemu-user.deb" "${qemu_pool}/qemu-user_${qemu_version}_amd64.deb"
  set_pin pin.qemu-user.version "$qemu_version"
  set_pin pin.qemu-user.sha256 "$(sha256sum "$tmp/qemu-user.deb" | cut -d ' ' -f 1)"
fi

# ALPINE
echo
echo "alpine (${MANIFEST})"
alpine_latest="$(fetch 'https://hub.docker.com/v2/repositories/library/alpine/tags?page_size=100' \
  | python3 -c 'import json, re, sys
tags = [t for t in json.load(sys.stdin)["results"] if re.fullmatch(r"\d+\.\d+\.\d+", t["name"])]
tags.sort(key=lambda t: [int(p) for p in t["name"].split(".")])
print(tags[-1]["name"], tags[-1]["digest"].removeprefix("sha256:")) if tags else None')"
if [ -z "$alpine_latest" ]; then
  echo "::error::could not read the latest alpine image"
  status=1
else
  set_pin pin.alpine.version "${alpine_latest% *}"
  set_pin pin.alpine.sha256 "${alpine_latest#* }"
fi

# ARM GNU TOOLCHAIN
echo
echo "arm-gnu (${MANIFEST})"
arm_api="https://gitlab.arm.com/api/v4/projects/tooling%2Fgnu-toolchains-for-arm"
arm_version="$(fetch "${arm_api}/repository/files/README.md/raw?ref=main" \
  | grep -oE 'Arm GNU Toolchain [0-9]+\.[0-9]+\.rel[0-9]+' | sed 's/^Arm GNU Toolchain //' | sort -uV | tail -1)"
if [ -z "$arm_version" ]; then
  echo "::error::could not read the latest Arm GNU Toolchain release"
  status=1
else
  arm_tar="arm-gnu-toolchain-${arm_version}-x86_64-aarch64_be-none-linux-gnu.tar.xz"
  set_pin pin.arm-gnu.version "$arm_version"
  set_pin pin.arm-gnu.sha256-aarch64-be \
    "$(fetch "${arm_api}/packages/generic/gnu-toolchain/${arm_version}/${arm_tar}.sha256asc" | cut -d ' ' -f 1)"
fi

# LLVM (WINDOWS)
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
