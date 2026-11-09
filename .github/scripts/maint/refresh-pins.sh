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

# STD (the benchmarks' std commit). Kept in benchmarks/std.rev
# rather than the manifest; a bump makes the next benchmark run
# compare nothing, as every workload's hash changes.
echo
echo "std (benchmarks/std.rev)"
std_old="$(cat benchmarks/std.rev)"
std_new="$(git ls-remote https://github.com/SamG101-Developer/SPP-STL.git HEAD 2> /dev/null | cut -f 1)"
if [ "${#std_new}" -ne 40 ]; then
  echo "::error::could not resolve HEAD for SPP-STL"
  status=1
elif [ "$std_old" = "$std_new" ]; then
  echo "  std.rev unchanged"
else
  echo "$std_new" > benchmarks/std.rev
  echo "  std.rev"
  echo "    ${std_old}"
  echo " -> ${std_new}"
fi

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
  for arch in x86_64:x86-64 aarch64:aarch64; do
    set_pin "pin.sccache.sha256-${arch#*:}" \
      "$(asset_digest mozilla/sccache "$sccache_tag" "sccache-${sccache_tag}-${arch%%:*}-pc-windows-msvc.tar.gz")"
  done
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

# RUST (stable; the run-time benchmarks' rustc). Hashed here rather
# than read from the .sha256 files beside them, like every other pin.
echo
echo "rust (${MANIFEST})"
rust_version="$(fetch https://static.rust-lang.org/dist/channel-rust-stable.toml \
  | python3 -c 'import sys, tomllib; print(tomllib.load(sys.stdin.buffer)["pkg"]["rust"]["version"].split()[0])')"
if [ -z "$rust_version" ]; then
  echo "::error::could not read the latest stable Rust release"
  status=1
else
  set_pin pin.rust.version "$rust_version"
  for component in rustc rust-std; do
    fetch -o "$tmp/${component}.tar.xz" \
      "https://static.rust-lang.org/dist/${component}-${rust_version}-x86_64-unknown-linux-gnu.tar.xz"
    set_pin "pin.rust.sha256-${component}" "$(sha256sum "$tmp/${component}.tar.xz" | cut -d ' ' -f 1)"
  done
fi

# QEMU (our fork's newest release; upstream-watch U17-U19)
echo
echo "qemu (${MANIFEST})"
qemu_repo="SamG101-Developer/qemu"
qemu_tag="$(fetch "${auth[@]}" "https://api.github.com/repos/${qemu_repo}/releases/latest" \
  | python3 -c 'import json, sys; print(json.load(sys.stdin).get("tag_name", ""))')"
qemu_digest=""
if [ -n "$qemu_tag" ]; then
  qemu_digest="$(asset_digest "$qemu_repo" "$qemu_tag" "qemu-user-static-${qemu_tag}-x86_64.tar.xz")"
fi
if [ -z "$qemu_digest" ]; then
  echo "::error::could not read the latest qemu-user release of ${qemu_repo}"
  status=1
else
  set_pin pin.qemu-user.tag "$qemu_tag"
  set_pin pin.qemu-user.sha256 "$qemu_digest"
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

# LLVM (WINDOWS): the newest final release of the major
# that [pin.llvm-lib] names, so a new major is never taken here.
# Both pins (x64, ARM64) move together, and only once every value
# resolves. macOS takes brew's llvm, so it has no pin here.
echo
echo "Windows LLVM (${MANIFEST})"
llvm_major="$(pinned pin.llvm-lib.version)"
llvm_tag="$(fetch "${auth[@]}" "https://api.github.com/repos/llvm/llvm-project/releases?per_page=100" \
  | python3 -c '
import json, re, sys
tags = [r["tag_name"] for r in json.load(sys.stdin) if not r["prerelease"] and not r["draft"]]
found = [m for t in tags if (m := re.fullmatch(r"llvmorg-" + sys.argv[1] + r"\.(\d+)\.(\d+)", t))]
if found:
    print(max(found, key=lambda m: (int(m[1]), int(m[2])))[0])
' "$llvm_major")"
if [ -z "$llvm_tag" ]; then
  echo "::error::could not find a final llvmorg-${llvm_major}.x.y release"
  status=1
else
  llvm_version="${llvm_tag#llvmorg-}"
  win_asset="clang+llvm-${llvm_version}-x86_64-pc-windows-msvc.tar.xz"
  win_arm_asset="clang+llvm-${llvm_version}-aarch64-pc-windows-msvc.tar.xz"
  win_digest="$(asset_digest llvm/llvm-project "$llvm_tag" "$win_asset")"
  win_arm_digest="$(asset_digest llvm/llvm-project "$llvm_tag" "$win_arm_asset")"
  if [ -z "$win_digest" ] || [ -z "$win_arm_digest" ]; then
    echo "::error::${llvm_tag} publishes no digest for ${win_asset} or ${win_arm_asset}; leaving the pins alone"
    echo "  pick a tag that ships both builds and move tag, asset and sha256 under [pin.llvm-win] and [pin.llvm-win-arm] by hand"
    status=1
  else
    set_pin pin.llvm-win.tag "$llvm_tag"
    set_pin pin.llvm-win.asset "$win_asset"
    set_pin pin.llvm-win.sha256 "$win_digest"
    set_pin pin.llvm-win-arm.tag "$llvm_tag"
    set_pin pin.llvm-win-arm.asset "$win_arm_asset"
    set_pin pin.llvm-win-arm.sha256 "$win_arm_digest"
  fi
fi

# XCODE (report only): [pin.xcode] holds one developer dir per
# macOS image and is deliberately not moved with the image's
# default, which is the drift it exists to stop. Read each image's
# list from actions/runner-images (the arm64 images CI runs on):
# a pin the image no longer ships fails CI, and a newer default is
# worth moving to by hand once a run on it passes.
echo
echo "Xcode (report only; ${MANIFEST})"
while IFS=$'\t' read -r image dir; do
  pinned_version="$(sed -nE 's|.*/Xcode_([0-9.]+)\.app/.*|\1|p' <<< "$dir")"
  readme="https://raw.githubusercontent.com/actions/runner-images/main/images/macos/${image}-arm64-Readme.md"
  listed="$(fetch "$readme" 2> /dev/null | python3 -c '
import re, sys
text = sys.stdin.read()
section = text.split("### Xcode", 1)[1].split("####", 1)[0] if "### Xcode" in text else ""
for row in section.splitlines():
    m = re.match(r"\|\s*([0-9][0-9.]*)\s*(\(default\))?\s*\|", row)
    if m:
        print(m[1], "default" if m[2] else "")
')"
  if [ -z "$listed" ] || [ -z "$pinned_version" ]; then
    echo "::error::could not read the Xcode list for ${image} (${readme}) or the version in ${dir}"
    status=1
    continue
  fi
  default_version="$(awk '$2 == "default" { print $1 }' <<< "$listed")"
  if ! awk '{ print $1 }' <<< "$listed" | grep -qxF "$pinned_version"; then
    echo "::error::${image} no longer ships Xcode ${pinned_version}; move pin.xcode.developer-dir-${image} to one it lists"
    echo "  it lists: $(awk '{ print $1 }' <<< "$listed" | tr '\n' ' ')"
    status=1
  elif [ "$default_version" = "$pinned_version" ]; then
    echo "  developer-dir-${image}: Xcode ${pinned_version}, the image's default"
  elif [ "$(printf '%s\n%s\n' "$pinned_version" "$default_version" | sort -V | tail -1)" = "$default_version" ]; then
    echo "  note: ${image} now defaults to Xcode ${default_version}; the pin is ${pinned_version}"
    echo "        move pin.xcode.developer-dir-${image} by hand once a run on ${default_version} passes"
  else
    echo "  developer-dir-${image}: Xcode ${pinned_version} (the image's default is the older ${default_version})"
  fi
done < <(python3 -c '
import tomllib
with open(".github/dependencies.toml", "rb") as f:
    for key, value in tomllib.load(f)["pin"]["xcode"].items():
        if key.startswith("developer-dir-"):
            print(key.removeprefix("developer-dir-"), value, sep="\t")
')

echo
if [ "$status" -eq 0 ]; then
  echo "done; review 'git diff' and commit"
else
  echo "finished with errors; see above" >&2
fi
exit "$status"
