#!/usr/bin/env bash
# Drive the gtest suite through gtest-parallel, writing
# one log per test.
set -euo pipefail

# Ensure the test binary can be found following the build
# stage. This is near enough guaranteed but a failsafe
# catches any edge case scenarios.
binary="${PWD}/build/tests/spp_tests"
cli="${PWD}/build/spp"
if [ "$RUNNER_OS" = "Windows" ]; then
  binary="${binary}.exe"
  cli="${cli}.exe"
fi
if ! [ -x "$binary" ]; then
  echo "::error::test binary not found at $binary"
  exit 1
fi
if ! [ -x "$cli" ]; then
  echo "::error::spp cli not found at $cli"
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

# Enforce the vcs checks here as-well as test boot, because
# I don't know where the failure is happening from, so just
# guard everywhere.
[ -f spp.toml ] || "$cli" init
[ -n "$(ls -A vcs 2>/dev/null)" ] || "$cli" vcs

if [ -z "$(find vcs -name '*.spp' -print -quit)" ]; then
  echo "::error::no .spp modules under ${work_dir}/vcs; the [vcs] clone did not land"
  exit 1
fi

# Run the parallel testing suite through the downloaded
# gtest-parallel script, setting the config options from
# the env flags.
python3 "$runner" \
  "$binary" \
  --output_dir="$log_dir" \
  ${WORKERS:+--workers="$WORKERS"}
