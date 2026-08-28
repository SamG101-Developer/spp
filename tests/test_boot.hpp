#pragma once

// Declarations only. The definitions live in "test_boot.cpp", which is what needs "spp.cli" and the parser to build a
// throwaway project - and importing those here made all ~120 test translation units pay for the whole compiler module
// graph to call two functions.
import spp.utils.types;
import std;

/**
 * Create the project fixture the whole suite shares, if it is not already on disk.
 *
 * gtest-parallel runs one process per test, so this runs once per worker rather than once per run, and a cross process
 * lock keeps the workers off each other. Running SppBootstrap.Fixture on its own first, as run-tests.sh does, keeps the
 * [vcs] clone out of the parallel phase entirely.
 */
auto ensure_temp_project() -> void;

/**
 * Compile one module of code as a throwaway project.
 * @param code The module source.
 * @param add_main Whether to prepend an empty "main", which an executable project needs.
 * @return The values the module's compile-time constants resolved to, by name. Reading a "cmp" back is the only way a
 * test can check what comp-time resolution computed rather than merely that it finished; see SPP_TEST_CMP_VALUES.
 */
auto build_temp_project(std::string code, bool add_main = true) -> spp::Map<spp::Str, spp::Str>;
