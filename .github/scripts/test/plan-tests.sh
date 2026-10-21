#!/usr/bin/env bash
# Expand a lane's test matrix: one entry per gtest mode and
# shard, then STL conformance, valgrind and the codegen checks
# where the lane asks for them, all against one build.
set -euo pipefail

# These land in GITHUB_OUTPUT as JSON, so they are
# validated rather than quoted.
[[ "$SHARD_COUNT" =~ ^[1-9][0-9]*$ ]] || { echo "::error::bad shard count '${SHARD_COUNT}'"; exit 1; }
for flag in STL VALGRIND CODEGEN; do
  [[ "${!flag}" =~ ^(true|false)$ ]] || { echo "::error::bad ${flag} flag '${!flag}'"; exit 1; }
done

read -ra mode_list <<< "$MODES"
[ "${#mode_list[@]}" -gt 0 ] || { echo "::error::no test modes"; exit 1; }

entries=()
add() {  # suite label timeout advisory [mode shard]
  entries+=("{\"suite\":\"$1\",\"label\":\"$2\",\"timeout\":$3,\"advisory\":$4,\"mode\":\"${5:-}\",\"shard\":${6:-0}}")
}

for mode in "${mode_list[@]}"; do
  case "$mode" in
    rel | dev) ;;
    *) echo "::error::bad test mode '${mode}'; expected rel or dev"; exit 1 ;;
  esac
  for ((shard = 0; shard < SHARD_COUNT; shard++)); do
    add gtest "gtest ${mode} $((shard + 1))/${SHARD_COUNT}" 60 false "$mode" "$shard"
  done
done

# STL conformance once per lane: it never reads SPP_TEST_MODE.
[ "$STL" = true ] && add stl "stl conformance" 20 false

# Linux only, and advisory until each has run clean for a while.
if [[ "$OS" == ubuntu* ]]; then
  [ "$VALGRIND" = true ] && add valgrind "valgrind groups" 30 true
  [ "$CODEGEN" = true ] && add codegen "codegen checks" 30 true
fi

tests="{\"include\":[$(IFS=,; echo "${entries[*]}")]}"
echo "tests=${tests}" >> "${GITHUB_OUTPUT:-/dev/stdout}"

echo "test jobs: ${#entries[@]}"
