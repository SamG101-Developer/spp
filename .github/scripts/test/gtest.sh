#!/usr/bin/env bash
# Drive the gtest suite through gtest-parallel, writing
# one log per test.
set -euo pipefail

# The build step guarantees this, but a missing binary reads
# far better here than as every test failing.
binary="${PWD}/build/tests/spp_tests"
[ "$RUNNER_OS" = "Windows" ] && binary="${PWD}/build/tests/spp_tests.exe"
if ! [ -x "$binary" ]; then
  echo "::error::test binary not found at $binary"
  exit 1
fi

# The fetch step is skipped on a cache hit alone, so an entry
# saved from a half-finished checkout leaves the directory in
# place with the script missing.
runner="${RUNNER_TEMP}/gtest-parallel/gtest-parallel"
if ! [ -f "$runner" ]; then
  echo "::error::gtest-parallel not found at $runner; the cache entry for GTEST_PARALLEL_COMMIT is incomplete."
  echo "::error::Delete it from the repository's Actions caches, or move the pin in .github/dependencies.toml."
  exit 1
fi

# Resolved before the cd below, so the logs stay where the
# upload steps expect them.
mkdir -p tests/test_output
log_dir="$(cd tests/test_output && pwd)"

# The tests read and write their fixture relative to the cwd,
# as .tools/run-unit-tests.sh does.
work_dir="${PWD}/tests/test_outputs"
mkdir -p "$work_dir"
cd "$work_dir"

# One process per test at cpu_count() workers, so wall-clock
# tracks physical cores and an SMT runner offers twice as many
# logical ones as it can really run.
cpus="$(python3 -c 'import multiprocessing; print(multiprocessing.cpu_count())')"

# A probe that cannot report is not worth failing the sweep for.
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
echo "cpu: ${cpus} logical, ${cores} physical (${model:-unknown model}); workers ${cpus}"

# gtest-parallel strides its enumeration rather than splitting by
# name, so the shards balance themselves and need no filter kept
# in step with the suite.
shard_count="${SHARD_COUNT:-1}"
shard_index="${SHARD_INDEX:-0}"
shard_flags=""
if [ "$shard_count" -gt 1 ]; then
  shard_flags="--shard_count=${shard_count} --shard_index=${shard_index}"
  echo "shard: ${shard_index} of ${shard_count}"
fi

# A binary that dies before main fails every test identically
# and says nothing; on macOS, have lldb print where.
if [ "$RUNNER_OS" = "macOS" ] && ! "$binary" --gtest_list_tests > /dev/null; then
  echo "::error::$binary crashed while listing its tests"
  otool -L "$binary" || true
  xcrun lldb --batch -o run -k bt -k quit -- "$binary" --gtest_list_tests || true
  exit 1
fi

# Seed the fixture serially, so the [vcs] clone happens once
# where git is the only thing that can fail, rather than inside
# whichever worker reaches it first.
"$binary" --gtest_filter=SppBootstrap.Fixture

status=0
# shellcheck disable=SC2086
python3 "$runner" \
  "$binary" \
  --output_dir="$log_dir" \
  $shard_flags || status=$?

# An empty vcs/ passes the compiler's structure validation, so
# on its own it surfaces only as every std symbol being
# undefined in every test.
if [ -z "$(find vcs -name '*.spp' -print -quit 2>/dev/null)" ]; then
  echo "::error::no .spp modules under ${work_dir}/vcs; the [vcs] clone did not land, so every std symbol was undefined"
  exit 1
fi

# Counted from the log directories: a crash leaves a log in failed/ but no FAILED line.
count() { find "${log_dir}/$1" -name '*.log' 2> /dev/null | wc -l | tr -d ' '; }
{
  echo "### gtest · ${SPP_TEST_MODE:-rel} · shard $((shard_index + 1)) of ${shard_count}"
  echo
  echo "passed $(count passed), failed $(count failed), interrupted $(count interrupted)"
  if [ "$(count failed)" -gt 0 ]; then
    echo
    echo '<details><summary>failed tests</summary>'
    echo
    find "${log_dir}/failed" -name '*.log' | sed 's|.*/[^-]*-||; s|-[0-9]*\.log$||' | sort | head -200 | sed 's/^/- /'
    echo
    echo '</details>'
  fi
} >> "${GITHUB_STEP_SUMMARY:-/dev/null}"

exit "$status"
