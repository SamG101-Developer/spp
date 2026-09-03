#include "test_macros.hpp"

/**
 * Create the shared project fixture, including the [vcs] clone, without compiling anything.
 *
 * gtest-parallel runs one process per test, so left to the suite this work happens inside whichever worker reaches it
 * first while the rest block on the fixture lock. run-tests.sh runs this test on its own before the parallel sweep, so
 * the clone is done serially, in a phase where a git failure is the only thing that can go wrong.
 */
TEST(SppBootstrap, Fixture) {
  ensure_temp_project();
}
