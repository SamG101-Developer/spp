#!/usr/bin/env bash
# Expand a lane's test matrix: one entry per gtest mode and
# shard, then STL conformance, valgrind, the codegen checks and
# the differential suite where the lane asks for them, all
# against one build. With PACK, those ride in the last gtest
# cell instead, where the test job runs every suite of a cell in
# parallel.
set -euo pipefail

# These land in GITHUB_OUTPUT as JSON, so they are
# validated rather than quoted.
[[ "$SHARD_COUNT" =~ ^[1-9][0-9]*$ ]] || { echo "::error::bad shard count '${SHARD_COUNT}'"; exit 1; }
for flag in STL VALGRIND CODEGEN DIFFERENTIAL PACK; do
  [[ "${!flag}" =~ ^(true|false)$ ]] || { echo "::error::bad ${flag} flag '${!flag}'"; exit 1; }
done

read -ra mode_list <<< "$MODES"
[ "${#mode_list[@]}" -gt 0 ] || { echo "::error::no test modes"; exit 1; }

# The suites beyond gtest: name, label, timeout, advisory.
# STL conformance runs once per lane: it never reads
# SPP_TEST_MODE. Valgrind, the codegen checks and the
# differential suite are Linux only, and advisory until each has
# run clean for a while.
extras=()
[ "$STL" = true ] && extras+=("stl|stl conformance|20|false")
if [[ "$OS" == ubuntu* ]]; then
  [ "$VALGRIND" = true ] && extras+=("valgrind|valgrind groups|30|true")
  [ "$CODEGEN" = true ] && extras+=("codegen|codegen checks|30|true")
  [ "$DIFFERENTIAL" = true ] && extras+=("differential|differential suite|30|true")
fi

entries=()
add() {  # suites label timeout advisory [mode shard]
  entries+=("{\"suites\":\"$1\",\"label\":\"$2\",\"timeout\":$3,\"advisory\":$4,\"mode\":\"${5:-}\",\"shard\":${6:-0}}")
}

last_mode="${mode_list[${#mode_list[@]} - 1]}"
for mode in "${mode_list[@]}"; do
  case "$mode" in
    rel | dev) ;;
    *) echo "::error::bad test mode '${mode}'; expected rel or dev"; exit 1 ;;
  esac
  for ((shard = 0; shard < SHARD_COUNT; shard++)); do
    suites="gtest"
    label="gtest ${mode} $((shard + 1))/${SHARD_COUNT}"
    timeout=60
    # A packed cell is advisory only if every suite in it is;
    # with gtest in it, it never is.
    if [ "$PACK" = true ] && [ "$mode" = "$last_mode" ] && [ "$shard" -eq $((SHARD_COUNT - 1)) ]; then
      for extra in ${extras[@]+"${extras[@]}"}; do
        IFS='|' read -r name name_label name_timeout _ <<< "$extra"
        suites+=" ${name}"
        label+=" + ${name_label%% *}"
        [ "$name_timeout" -gt "$timeout" ] && timeout="$name_timeout"
      done
    fi
    add "$suites" "$label" "$timeout" false "$mode" "$shard"
  done
done

if [ "$PACK" = false ]; then
  for extra in ${extras[@]+"${extras[@]}"}; do
    IFS='|' read -r name name_label name_timeout name_advisory <<< "$extra"
    add "$name" "$name_label" "$name_timeout" "$name_advisory"
  done
fi

tests="{\"include\":[$(IFS=,; echo "${entries[*]}")]}"
echo "tests=${tests}" >> "${GITHUB_OUTPUT:-/dev/stdout}"

echo "test jobs: ${#entries[@]}"
