#!/usr/bin/env bash
# Copy the reproducer files a crashing compiler names in the build log into build-logs/crash, so the failed-build
# upload carries them. clang and clang-cl list them after "PLEASE ATTACH THE FOLLOWING CRASH REPRODUCER FILES"; GCC
# names one only under -freport-bug. A module compile's reproducer also needs the BMIs its script names
# (-fmodule-file=NAME=PATH, relative to the build tree): they are copied to the same relative paths under
# build-logs/crash, so the script replays from that folder as it is.
set -euo pipefail

log="build-logs/build.log"
out="build-logs/crash"
build="build"
[ -f "$log" ] || exit 0

paths="$(
  {
    sed -nE 's/.*diagnostic msg: (.+\.(sh|c|cc|cpp|cppm|ii|i|ixx))[[:space:]]*$/\1/p' "$log"
    sed -nE 's/.*Preprocessed source stored into (.+) file.*/\1/p' "$log"
  } | tr -d '\r' | sort -u
)"
[ -n "$paths" ] || exit 0

mkdir -p "$out"
while IFS= read -r path; do
  if [ "$RUNNER_OS" = "Windows" ]; then path="$(cygpath -u "$path")"; fi
  if [ -f "$path" ]; then
    cp "$path" "$out/"
    echo "collected $path"
  else
    echo "missing $path" >&2
  fi
done <<< "$paths"

for script in "$out"/*.sh; do
  [ -f "$script" ] || continue
  grep -oE '"-fmodule-file=[^"=]+=[^"]+"' "$script" | sed -E 's/^"-fmodule-file=[^=]+=(.*)"$/\1/' | sort -u \
    | while IFS= read -r bmi; do
        rel="${bmi//\\\\//}"
        rel="${rel//\\//}"
        case "$rel" in /* | [A-Za-z]:*) echo "skipped absolute BMI $bmi" >&2; continue ;; esac
        if [ -f "$build/$rel" ]; then
          mkdir -p "$out/$(dirname "$rel")"
          cp "$build/$rel" "$out/$rel"
        else
          echo "missing BMI $build/$rel" >&2
        fi
      done
  echo "collected the BMIs for $(basename "$script")"
done
