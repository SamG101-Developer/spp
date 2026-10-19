#include "test_macros.hpp"

/**
 * Create the shared project fixture, including the [vcs] clone, without compiling anything.
 *
 * gtest-parallel runs one process per test, so left to the suite this work happens inside whichever worker reaches it
 * first while the rest block on the fixture lock. tests/compiler/run.sh runs this test on its own before the parallel
 * sweep, so the clone is done serially, in a phase where a git failure is the only thing that can go wrong. It also
 * pulls an existing checkout, so each run compiles against the current std rather than whichever one was cloned first.
 */
TEST(SppBootstrap, Fixture) {
  EXPECT_TRUE(EnsureTempProject(true)) << "the [vcs] pull failed; the suite runs on the existing checkout";
}
