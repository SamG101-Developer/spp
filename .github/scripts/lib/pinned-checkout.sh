# Check a repository out at one pinned commit, with a depth-1 fetch.
# shellcheck shell=bash

# pinned_checkout <dir> <url> <40-hex commit>
pinned_checkout() {
  local dir="$1" url="$2" sha="$3" got

  if ! [[ "$sha" =~ ^[0-9a-f]{40}$ ]]; then
    echo "::error::${url} is not pinned to a full commit: '${sha}'"
    return 1
  fi

  git init -q "$dir"
  git -C "$dir" remote add origin "$url"
  git -C "$dir" fetch -q --depth 1 origin "$sha"
  git -C "$dir" checkout -q --detach FETCH_HEAD

  # FETCH_HEAD is whatever the server answered with; hold it to the pin.
  got="$(git -C "$dir" rev-parse HEAD)"
  if [ "$got" != "$sha" ]; then
    echo "::error::${url} checked out ${got}, not the pinned ${sha}"
    return 1
  fi
  echo "${dir##*/} @ ${sha}"
}
