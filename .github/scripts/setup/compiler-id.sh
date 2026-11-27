#!/usr/bin/env bash
# Identify the compiler CXX names by its version banner, for the
# key of a cache holding what it built. A runner label can move
# to a new compiler (windows-2025 went to VS 2026's MSVC), and a
# module library built by the old one does not link against the
# new one: the mangled names differ.
set -euo pipefail

cxx="${CXX:-c++}"
if ! command -v "$cxx" > /dev/null; then
  echo "compiler-id: no compiler ${cxx}" >&2
  exit 1
fi
case "$(basename "$cxx")" in
  # cl has no --version: run bare, it prints its banner to stderr.
  cl | cl.exe) banner="$("$cxx" 2>&1 >/dev/null | head -1 || true)" ;;
  *) banner="$("$cxx" --version 2>&1 | head -1 || true)" ;;
esac
banner="${banner%$'\r'}"

if [ -z "$banner" ]; then
  echo "compiler-id: no version banner from ${cxx}" >&2
  exit 1
fi

echo "compiler: ${banner}"
echo "id=$(printf '%s' "$banner" | sha256sum | cut -c1-12)" >> "$GITHUB_OUTPUT"
