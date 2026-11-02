#!/usr/bin/env bash
# Add each record in the folder $1 (compile.json, runtime.json) to
# the benchmarks branch as records/SHA/<kind>.json, and as
# latest/<kind>.json, the newest master record of that kind. The
# first run makes the branch, as an orphan. Needs GH_TOKEN
# (contents: write), GH_REPO and SHA.
set -euo pipefail

records="$1"
: "${GH_TOKEN:?}" "${GH_REPO:?}" "${SHA:?}"
branch="benchmarks"
work="${RUNNER_TEMP}/${branch}-branch"

shopt -s nullglob
files=("${records}"/*.json)
if (( ${#files[@]} == 0 )); then
  echo "::error::no records in ${records}"
  exit 1
fi

# The header actions/checkout uses; masked, as only the raw token is.
basic="$(printf 'x-access-token:%s' "$GH_TOKEN" | base64 -w0)"
echo "::add-mask::${basic}"
auth="AUTHORIZATION: basic ${basic}"

git init --quiet "$work"
git -C "$work" remote add origin "https://github.com/${GH_REPO}.git"
if git -C "$work" -c http.extraheader="$auth" fetch --quiet --depth=1 origin "$branch" 2>/dev/null; then
  git -C "$work" checkout --quiet -b "$branch" FETCH_HEAD
else
  git -C "$work" checkout --quiet --orphan "$branch"
  cat > "${work}/README.md" <<'EOF'
# Benchmark records

Written by post_merge.yaml, never by hand. `records/<sha>/<kind>.json`
is the record of each master commit that changed the compiler, from
benchmarks/run.py (`compile`) and benchmarks/runtime.py (`runtime`),
and `latest/<kind>.json` is the newest of each. A pull request
compares against its base commit's records. See
.triages/docs_open/benchmark-ci-plan.md on master.
EOF
fi

mkdir -p "${work}/records/${SHA}" "${work}/latest"
for file in "${files[@]}"; do
  cp "$file" "${work}/records/${SHA}/$(basename "$file")"
  cp "$file" "${work}/latest/$(basename "$file")"
done

git -C "$work" add --all
git -C "$work" \
  -c user.name="github-actions[bot]" \
  -c user.email="41898282+github-actions[bot]@users.noreply.github.com" \
  commit --quiet -m "Records of ${SHA}"
git -C "$work" -c http.extraheader="$auth" push --quiet origin "HEAD:refs/heads/${branch}"
echo "Published ${#files[@]} record(s) of ${SHA} to ${branch}."
