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

# The sweep is one process per test at cpu_count() workers, so
# its wall-clock tracks physical cores, and an SMT runner offers
# twice as many logical ones as it can really run.
cpus="$(python3 -c 'import multiprocessing; print(multiprocessing.cpu_count())')"

# A probe that cannot report is not worth failing the sweep for,
# so each assignment carries its own fallback rather than letting
# a missing lscpu take the step down.
cores="?"
model=""
case "$RUNNER_OS" in
  Linux)
    cores="$(lscpu -p=core 2> /dev/null | awk '!/^#/ && !seen[$0]++ { n++ } END { print n + 0 }')" || cores="?"
    model="$(lscpu 2> /dev/null | sed -n 's/^Model name: *//p')" || model=""
    ;;
  macOS)
    cores="$(sysctl -n hw.physicalcpu 2> /dev/null)" || cores="?"
    model="$(sysctl -n machdep.cpu.brand_string 2> /dev/null)" || model=""
    ;;
esac
echo "cpu: ${cpus} logical, ${cores} physical (${model:-unknown model}); workers ${WORKERS:-$cpus}"

# gtest-parallel strides its enumeration rather than splitting it
# by name, so the shards balance themselves and need no filter
# kept in step with the suite. One shard means the whole sweep,
# and the flags are left off entirely.
shard_count="${SHARD_COUNT:-1}"
shard_index="${SHARD_INDEX:-0}"
shard_flags=""
if [ "$shard_count" -gt 1 ]; then
  shard_flags="--shard_count=${shard_count} --shard_index=${shard_index}"
  echo "shard: ${shard_index} of ${shard_count}"
fi

# Seed the fixture serially before the parallel sweep, so the
# [vcs] clone happens once in a phase where git is the only
# thing that can fail, rather than inside whichever worker
# process reaches it first.
"$binary" --gtest_filter=SppBootstrap.Fixture

# Run the parallel testing suite through the downloaded
# gtest-parallel script, setting the config options from
# the env flags.
status=0
# shellcheck disable=SC2086
python3 "$runner" \
  "$binary" \
  --output_dir="$log_dir" \
  $shard_flags \
  ${WORKERS:+--workers="$WORKERS"} || status=$?

# Guard here as well as in test boot: an empty vcs/ passes the
# compiler's structure validation, so on its own it surfaces
# only as every std symbol being undefined in every test.
if [ -z "$(find vcs -name '*.spp' -print -quit 2>/dev/null)" ]; then
  echo "::error::no .spp modules under ${work_dir}/vcs; the [vcs] clone did not land, so every std symbol was undefined"
  exit 1
fi

exit "$status"
