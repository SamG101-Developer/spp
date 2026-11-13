#!/usr/bin/env bash
# Run clang-tidy with the repo's .clang-tidy over a clang build
# tree, which must be built: clang-tidy loads its module files.
# Usage: clang-tidy.sh <build-dir> [file ... | --since <sha>];
# no files means every project file, --since the files changed
# after <sha> (all of them if a header or .clang-tidy changed).
# Findings are de-duplicated into clang-tidy-report.txt at the
# repo root, and annotated in CI. Exits 1 on any finding.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
build_dir="$(cd "$1" && pwd)"
shift

if [[ "${1:-}" == "--since" ]]; then
  since="$2"
  changed="$(git -C "${root}" diff --name-only --diff-filter=ACMR "${since}"...HEAD)"
  set --
  if ! grep -qE '(\.hpp|\.clang-tidy)$' <<< "${changed}"; then
    mapfile -t files < <(grep -E '^(headers/.+\.ixx|sources/.+\.cpp|tests/.+\.cpp|main\.cpp)$' <<< "${changed}" || true)
    if ((${#files[@]} == 0)); then
      echo "No C++ files changed since ${since}"
      exit 0
    fi
    set -- "${files[@]}"
  fi
fi
version="${LLVM_LIB_VERSION:-23}"
report="${root}/clang-tidy-report.txt"
log="$(mktemp -t clang-tidy.XXXXXX)"
trap 'rm -f "${log}"' EXIT

# run-clang-tidy takes regexes over compile_commands.json's paths.
if (($# == 0)); then
  filter="^${root}/(headers|sources|tests|main\.cpp)"
else
  filter="^${root}/($(printf '%s\n' "$@" | sed 's/\./\\./g' | paste -sd '|'))$"
fi

# Progress lines go to the terminal; the findings are read back from the log.
"run-clang-tidy-${version}" -p "${build_dir}" -clang-tidy-binary "clang-tidy-${version}" -j "$(nproc)" -quiet \
  "${filter}" 2>&1 \
  | tee "${log}" \
  | grep --line-buffered -E '^\[ *[0-9]+/[0-9]+\]' || true

# A header's findings repeat once per file that includes it.
grep -E '^/.+:[0-9]+:[0-9]+: (warning|error): .*\]$' "${log}" | sort -u > "${report}" || true

if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
  sed -E "s#^${root}/##" "${report}" \
    | sed -E 's/^([^:]+):([0-9]+):([0-9]+): (warning|error): (.*) \[([^]]+)\]$/::warning file=\1,line=\2,col=\3,title=\6::\5/'
fi

sed -E 's/.*\[([^]]+)\]$/\1/' "${report}" | sort | uniq -c | sort -rn
echo "Wrote ${report} ($(wc -l < "${report}") findings)"
[[ ! -s "${report}" ]]
