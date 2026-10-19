#!/usr/bin/env bash
# Expand a lane's gtest matrix: one entry per mode and
# shard, all against one build.
set -euo pipefail

# These land in GITHUB_OUTPUT as JSON, so they are
# validated rather than quoted.
[[ "$SHARD_COUNT" =~ ^[1-9][0-9]*$ ]] || { echo "::error::bad shard count '${SHARD_COUNT}'"; exit 1; }

read -ra mode_list <<< "$MODES"
[ "${#mode_list[@]}" -gt 0 ] || { echo "::error::no test modes"; exit 1; }

entries=()
for mode in "${mode_list[@]}"; do
  case "$mode" in
    rel | dev) ;;
    *) echo "::error::bad test mode '${mode}'; expected rel or dev"; exit 1 ;;
  esac
  for ((shard = 0; shard < SHARD_COUNT; shard++)); do
    entries+=("{\"mode\":\"${mode}\",\"shard\":${shard},\"label\":\"${mode} $((shard + 1))/${SHARD_COUNT}\"}")
  done
done

tests="{\"include\":[$(IFS=,; echo "${entries[*]}")]}"
echo "tests=${tests}" >> "${GITHUB_OUTPUT:-/dev/stdout}"

echo "gtest jobs: ${#entries[@]} (${MODES} x ${SHARD_COUNT} shards)"
