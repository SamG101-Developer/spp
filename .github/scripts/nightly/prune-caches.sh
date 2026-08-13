#!/usr/bin/env bash
# Delete the Actions caches that can never be restored again, so they stop counting against the repository's 10 GB
# budget while they wait for eviction.
set -euo pipefail

# Delete every cache the given jq filter picks out of `gh cache list`, which pages at 100 entries: re-list until a pass
# finds nothing left. The bound stops a delete that silently fails from spinning forever.
prune() {
  local filter="$1"
  shift
  local stale id key
  for _ in $(seq 10); do
    stale="$(gh cache list --limit 100 --json id,key,ref,createdAt | jq -r "$@" "$filter")"
    [ -n "$stale" ] || break
    while IFS=$'\t' read -r id key; do
      echo "deleting ${key}"
      gh cache delete "$id" || echo "::warning::could not delete ${key}"
    done <<< "$stale"
  done
}

# Every cache this repository writes now ends in a segment that changes whenever its contents should: a commit sha for
# the compiler caches, which are reached through a prefix restore-key that picks the newest match, and a hash of the
# defining files for the rest.
for prefix in spp-libs- cc- doxygen-; do
  echo "keeping the newest ${prefix} cache per branch and key prefix"

  # shellcheck disable=SC2016  # $p is a jq variable bound by --arg below; the shell must not expand it.
  prune '[.[] | select(.key | startswith($p))]
         | group_by([.ref, (.key | sub("-[^-]*$"; ""))])
         | map(sort_by(.createdAt) | .[:-1])
         | flatten | .[] | [.id, "\(.key) on \(.ref)"] | @tsv' \
    --arg p "$prefix"
done
