#!/usr/bin/env bash
# Merge a coverage lane's profiles and write its reports: llvm-cov's text for Sonar, lcov, and the totals.
set -euo pipefail

profdata="llvm-profdata-${LLVM_LIB_VERSION}"
cov="llvm-cov-${LLVM_LIB_VERSION}"
prof_dir="${RUNNER_TEMP}/prof"
out="coverage"
ignore='(^|/)(third_party|tests|build)/|^/usr/'

shopt -s nullglob
profiles=("${prof_dir}"/*.profraw)
if [ "${#profiles[@]}" -eq 0 ]; then
  echo "::error::no profiles under ${prof_dir}"
  exit 1
fi
echo "merging ${#profiles[@]} profiles"

mkdir -p "$out"
"$profdata" merge -sparse -o "${out}/spp.profdata" "${profiles[@]}"

binaries=(build/spp -object build/tests/spp_tests)
"$cov" show --show-branches=count --instr-profile "${out}/spp.profdata" -ignore-filename-regex="$ignore" \
  "${binaries[@]}" > "${out}/coverage.txt"
"$cov" export -format=lcov --instr-profile "${out}/spp.profdata" -ignore-filename-regex="$ignore" \
  "${binaries[@]}" > "${out}/coverage.lcov"
"$cov" report --instr-profile "${out}/spp.profdata" -ignore-filename-regex="$ignore" \
  "${binaries[@]}" > "${out}/report.txt"

total="$(grep '^TOTAL' "${out}/report.txt")"
read -r -a cols <<< "$total"
{
  echo "### Coverage"
  echo
  echo "| regions | functions | lines | branches |"
  echo "| --- | --- | --- | --- |"
  echo "| ${cols[3]} | ${cols[6]} | ${cols[9]} | ${cols[12]} |"
} >> "${GITHUB_STEP_SUMMARY:-/dev/stdout}"
echo "$total"
