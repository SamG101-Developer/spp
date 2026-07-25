#!/usr/bin/env bash
# Delete the Actions caches that can never be restored again, so they stop counting against the repository's 10 GB
# budget while they wait for eviction.
set -euo pipefail

# Delete every cache the given jq filter picks out of `gh cache list`, which pages at 100 entries: re-list until a pass
# finds nothing left. The bound stops a delete that silently fails from spinning forever.
prune() {
  local filter="$1" stale id key
  for _ in $(seq 10); do
    stale="$(gh cache list --limit 100 --json id,key,ref,createdAt --jq "$filter")"
    [ -n "$stale" ] || break
    while IFS=$'\t' read -r id key; do
      echo "deleting ${key}"
      gh cache delete "$id" || echo "::warning::could not delete ${key}"
    done <<< "$stale"
  done
}

# Read a pinned value straight out of .github/versions.env. The loader is not used here: this job installs no toolchain,
# so it has no reason to publish the whole file into the environment.
pinned() { grep -oP "^$1=\K.*" .github/versions.env; }

# The dependency cache key carries the UTC date and SPP_LIBS_CACHE_VERSION (see .github/actions/setup-toolchain), so the
# moment either changes the previous entries are unreachable.
version="$(pinned SPP_LIBS_CACHE_VERSION)"
keep="-$(date -u +%Y-%m-%d)-v${version}"
echo "keeping spp-libs caches ending in '${keep}'"
prune ".[] | select(.key | startswith(\"spp-libs-\")) | select(.key | endswith(\"${keep}\") | not) | [.id, .key] | @tsv"

# The compiler caches end in a commit sha and are restored through a prefix restore-key that picks the newest match, so
# only the newest entry behind a given prefix can ever be hit again. Group on (ref, prefix), keep the newest of each
# group, delete what is behind it.
echo "keeping the newest cc- cache per branch and key prefix"
prune '[.[] | select(.key | startswith("cc-"))]
       | group_by([.ref, (.key | sub("-[^-]*$"; ""))])
       | map(sort_by(.createdAt) | .[:-1])
       | flatten | .[] | [.id, "\(.key) on \(.ref)"] | @tsv'

# The doxygen cache key carries the release version and DOXYGEN_CACHE_VERSION, both pinned in versions.env, so exactly
# one entry per runner is reachable and every other one is dead.
keep="-$(pinned DOXYGEN_VERSION)-v$(pinned DOXYGEN_CACHE_VERSION)"
echo "keeping doxygen caches ending in '${keep}'"
prune ".[] | select(.key | startswith(\"doxygen-\")) | select(.key | endswith(\"${keep}\") | not) | [.id, .key] | @tsv"
