#include "test_boot.hpp"

import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.cli;
import spp.lex.lexer;
import spp.parse.parser_spp;
import spp.parse.errors.parser_error;
import spp.utils.files;
import spp.utils.types;
import std;
import sys;

/**
 * Create the project fixture the whole suite shares, if it is not already on disk.
 *
 * gtest-parallel runs one process per test, so this runs once per worker rather than once per run, and the cross
 * process lock below is what keeps the workers off each other. Running SppBootstrap.Fixture on its own first, as
 * run-tests.sh does, keeps the [vcs] clone out of the parallel phase entirely.
 */
auto ensure_temp_project() -> void {
  const auto cwd = std::filesystem::current_path();
  constexpr auto fp = "../../tests/test_outputs";

  // Ensure the output directory exists before locking it.
  std::filesystem::create_directories(cwd / fp);

  // Serialize initialization (handle_init + handle_vcs) across
  // parallel test workers. The lock lives in the temp directory
  // rather than inside the fixture, because handle_init refuses
  // to run in a directory that is not empty -- and it is a file
  // rather than the fixture directory itself, because Windows
  // cannot lock a directory at all. Workers of one run share a
  // machine, so a machine-wide lock file is enough.
  auto init_lock = spp::utils::files::FileLock();
  init_lock.LockExclusive(std::filesystem::temp_directory_path() / "spp-test-fixture.lock");

  // Temporary enforcement check because the GitHub runner has
  // some strange behaviour with not cloning the vcs libraries
  // for the unit tests (the stl),
  const auto vcs_empty = [&] {
    const auto vcs = cwd / fp / "vcs";
    return not std::filesystem::exists(vcs) or std::filesystem::is_empty(vcs);
  };
  std::filesystem::current_path(cwd / fp);
  if (not std::filesystem::exists("spp.toml")) { spp::cli::handle_init(); }
  if (vcs_empty()) { spp::cli::handle_vcs(); }
  std::filesystem::current_path(cwd);

  init_lock.Unlock();

  // Failure if the vcs pull failed, stopping the test suite early
  // as there is no point running it without the stl linked.
  if (vcs_empty()) {
    std::cerr << "FATAL: no [vcs] dependencies in the test fixture; the clone into vcs/ failed.\n";
    std::exit(1);
  }

  std::filesystem::create_directories(cwd / fp / "src");
}

/**
 * The s++ build mode the suite compiles its fixtures in. "rel" (O3) unless SPP_TEST_MODE says otherwise, which is what
 * lets CI run the same suite over the O0 pipeline as a second matrix entry. Read once: every test in a process wants
 * the same answer, and an unrecognised value is a typo worth failing on rather than silently compiling the other mode.
 */
namespace {
  auto test_build_mode() -> spp::Str const& {
    static const auto mode = [] {
      const auto *env = std::getenv("SPP_TEST_MODE");
      auto value = spp::Str(env != nullptr ? env : "rel");
      if (value != "dev" and value != "rel") {
        std::cerr << "SPP_TEST_MODE must be 'dev' or 'rel', not '" << value << "'\n";
        std::abort();
      }
      return value;
    }();
    return mode;
  }
}

/**
 * Compile one module of code as a throwaway project.
 * @param code The module source.
 * @param add_main Whether to prepend an empty "main", which an executable project needs.
 * @return The values the module's compile-time constants resolved to, by name. Reading a "cmp" back is the only way a
 * test can check what comp-time resolution computed rather than merely that it finished; see SPP_TEST_CMP_VALUES.
 */
auto build_temp_project(std::string code, const bool add_main) -> spp::Map<spp::Str, spp::Str> {
  const auto cwd = std::filesystem::current_path();
  constexpr auto fp = "../../tests/test_outputs";

  if (add_main) {
    code = "fun main() -> Void { }\n" + code;
  }

  ensure_temp_project();

  // Build the project.
  std::filesystem::current_path(cwd / fp);
  auto comp_time_constants = spp::Map<spp::Str, spp::Str>();
  try {
    comp_time_constants = spp::cli::run_cpp_google_test(test_build_mode(), std::move(code));
  }
  catch (const spp::analyse::errors::SemanticError &e) {
    std::cout << e.what() << std::endl;
    spp::analyse::scopes::ScopeManager::Cleanup();
    throw;
  }
  catch (const spp::parse::errors::SppSyntaxError &e) {
    std::cout << e.what() << std::endl;
    spp::analyse::scopes::ScopeManager::Cleanup();
    throw;
  }
  catch (const std::exception &e) {
    std::cout << e.what() << std::endl;
    spp::analyse::scopes::ScopeManager::Cleanup();
    throw;
  }

  spp::analyse::scopes::ScopeManager::Cleanup();
  std::filesystem::current_path(cwd);
  return comp_time_constants;
}
