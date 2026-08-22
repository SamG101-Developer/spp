#!/usr/bin/env bash
# Drive the gtest suite through gtest-parallel, writing
# one log per test.
set -euo pipefail

# Ensure the test binary can be found following the build
# stage. This is near enough guaranteed but a failsafe
# catches any edge case scenarios.
binary="${PWD}/build/tests/spp_tests"
[ "$RUNNER_OS" = "Windows" ] && binary="${PWD}/build/tests/spp_tests.exe"
if ! [ -x "$binary" ]; then
  echo "::error::test binary not found at $binary"
  exit 1
fi

# Ensure the gtest-parallel test runner script itself is
# present. The fetch step is skipped on a cache hit alone,
# so a cache entry that was saved from a half-finished
# checkout leaves the directory in place with the script
# missing.
runner="${RUNNER_TEMP}/gtest-parallel/gtest-parallel"
if ! [ -f "$runner" ]; then
  echo "::error::gtest-parallel not found at $runner; the cache entry for GTEST_PARALLEL_COMMIT is incomplete."
  echo "::error::Delete it from the repository's Actions caches, or move the pin in .github/dependencies.toml."
  exit 1
fi

# Resolve the log directory before the cd below, so it stays
# where the artefact upload step expects it.
mkdir -p "$OUTPUT_DIR"
log_dir="$(cd "$OUTPUT_DIR" && pwd)"

# The tests read and write their project fixture relative to
# the cwd, so they must run from tests/test_outputs; this
# mirrors .tools/run-unit-tests.sh.
work_dir="${PWD}/tests/test_outputs"
mkdir -p "$work_dir"
cd "$work_dir"

# Run the parallel testing suite through the downloaded
# gtest-parallel script, setting the config options from
# the env flags. The fixture, including the [vcs] clone, is
# created by the first test to reach build_temp_project;
# there is no CLI seed step because build/spp is not wired
# to run_cli.
status=0
python3 "$runner" \
  "$binary" \
  --output_dir="$log_dir" \
  ${WORKERS:+--workers="$WORKERS"} || status=$?

# Guard here as well as in test boot: an empty vcs/ passes the
# compiler's structure validation, so on its own it surfaces
# only as every std symbol being undefined in every test.
if [ -z "$(find vcs -name '*.spp' -print -quit 2>/dev/null)" ]; then
  echo "::error::no .spp modules under ${work_dir}/vcs; the [vcs] clone did not land, so every std symbol was undefined"
  exit 1
fi

exit "$status"
