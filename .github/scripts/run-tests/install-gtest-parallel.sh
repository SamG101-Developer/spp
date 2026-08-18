#!/usr/bin/env bash
# Check gtest-parallel out at its pinned commit.
set -euo pipefail

dest="${RUNNER_TEMP}/gtest-parallel"

git init -q "$dest"
git -C "$dest" remote add origin https://github.com/google/gtest-parallel.git
git -C "$dest" fetch -q --depth 1 origin "$GTEST_PARALLEL_COMMIT"
git -C "$dest" checkout -q --detach FETCH_HEAD
echo "gtest-parallel @ ${GTEST_PARALLEL_COMMIT}"
