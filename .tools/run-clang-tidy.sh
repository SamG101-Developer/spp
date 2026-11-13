#!/usr/bin/env bash
set -euo pipefail

# Run clang-tidy over the local clang build tree (see .github/scripts/scan/clang-tidy.sh). Arguments are files
# relative to the repo root (default: every project file). Needs a current clang build.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec "${ROOT}/.github/scripts/scan/clang-tidy.sh" "${ROOT}/cmake-build-release-wsl-clang" "$@"
