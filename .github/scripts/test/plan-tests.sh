#!/usr/bin/env bash
# Expand a lane's test matrix: one gtest cell per mode, and per
# shard where the lane shards (each carrying the shard count),
# then one cell for the rest - STL conformance, the codegen
# checks, and on Linux valgrind and the differential suite -
# which the test job runs side by side, all against one build.
set -euo pipefail

# These land in GITHUB_OUTPUT as JSON, so they are
# validated rather than quoted.
[[ "$SHARD_COUNT" =~ ^[1-9][0-9]*$ ]] || { echo "::error::bad shard count '${SHARD_COUNT}'"; exit 1; }
for flag in STL VALGRIND CODEGEN DIFFERENTIAL; do
  [[ "${!flag}" =~ ^(true|false)$ ]] || { echo "::error::bad ${flag} flag '${!flag}'"; exit 1; }
done

read -ra mode_list <<< "$MODES"
[ "${#mode_list[@]}" -gt 0 ] || { echo "::error::no test modes"; exit 1; }

# The suites beyond gtest: name, label, timeout, advisory.
# STL conformance runs once per lane: it never reads
# SPP_TEST_MODE. Valgrind and the differential suite are Linux
# only. The codegen checks run on every lane, each check on the
# platforms its "# platforms:" line names (tests/codegen/run.sh).
# All three are advisory until each has run clean for a while.
extras=()
[ "$STL" = true ] && extras+=("stl|STL Conformance|90|false")
[[ "$OS" == ubuntu* && "$VALGRIND" = true ]] && extras+=("valgrind|Valgrind Conformance|90|true")
[ "$CODEGEN" = true ] && extras+=("codegen|Codegen Checks|90|true")
[[ "$OS" == ubuntu* && "$DIFFERENTIAL" = true ]] && extras+=("differential|Differential Suite|90|true")

entries=()
add() {  # suites label timeout advisory [mode shard]
  entries+=("{\"suites\":\"$1\",\"label\":\"$2\",\"timeout\":$3,\"advisory\":$4,\"mode\":\"${5:-}\",\"shard\":${6:-0},\"shards\":${SHARD_COUNT}}")
}

for mode in "${mode_list[@]}"; do
  case "$mode" in
    rel) mode_label=Rel ;;
    dev) mode_label=Dev ;;
    *) echo "::error::bad test mode '${mode}'; expected rel or dev"; exit 1 ;;
  esac
  for ((shard = 0; shard < SHARD_COUNT; shard++)); do
    label="GTest ${mode_label}"
    [ "$SHARD_COUNT" -gt 1 ] && label+=" $((shard + 1))/${SHARD_COUNT}"
    add gtest "$label" 90 false "$mode" "$shard"
  done
done

# The rest share one cell, named by each suite's first word
# ("STL + Valgrind + Codegen + Differential"), or its whole
# label when alone. It is advisory only if every suite in it
# is; an advisory suite beside a required one stays advisory
# through its own step's continue-on-error.
if [ "${#extras[@]}" -gt 0 ]; then
  suites="" label="" timeout=0 advisory=true
  for extra in "${extras[@]}"; do
    IFS='|' read -r name name_label name_timeout name_advisory <<< "$extra"
    suites+="${suites:+ }${name}"
    label+="${label:+ + }${name_label%% *}"
    [ "$name_timeout" -gt "$timeout" ] && timeout="$name_timeout"
    [ "$name_advisory" = false ] && advisory=false
  done
  [ "${#extras[@]}" -eq 1 ] && label="$name_label"
  add "$suites" "$label" "$timeout" "$advisory"
fi

tests="{\"include\":[$(IFS=,; echo "${entries[*]}")]}"
echo "tests=${tests}" >> "${GITHUB_OUTPUT:-/dev/stdout}"

echo "test jobs: ${#entries[@]}"
