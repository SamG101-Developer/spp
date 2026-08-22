#pragma once

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
 * Compile one module of code as a throwaway project.
 * @param code The module source.
 * @param add_main Whether to prepend an empty "main", which an executable project needs.
 * @return The values the module's compile-time constants resolved to, by name. Reading a "cmp" back is the only way a
 * test can check what comp-time resolution computed rather than merely that it finished; see SPP_TEST_CMP_VALUES.
 */
inline auto build_temp_project(std::string code, const bool add_main = true) -> spp::Map<spp::Str, spp::Str> {
  const auto cwd = std::filesystem::current_path();
  constexpr auto fp = "../../tests/test_outputs";

  if (add_main) {
    code = "fun main() -> Void { }\n" + code;
  }

  // Ensure the output directory exists before locking it.
  std::filesystem::create_directories(cwd / fp);

  // Serialize initialization (handle_init + handle_vcs) across
  // parallel test workers. The lock is taken on the project
  // directory itself rather than a lock file inside it, because
  // handle_init refuses to run in a directory that is not empty.
  const auto lock_path = spp::utils::files::NativeString(cwd / fp);
  const int init_lock_fd = sys::open(lock_path.c_str(), sys::O_RDONLY);
  sys::flock(init_lock_fd, sys::LOCK_EX);

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

  sys::flock(init_lock_fd, sys::LOCK_UN);
  sys::close(init_lock_fd);

  // Failure if the vcs pull failed, stopping the test suite early
  // as there is no point running it without the stl linked.
  if (vcs_empty()) {
    std::cerr << "FATAL: no [vcs] dependencies in the test fixture; the clone into vcs/ failed.\n";
    std::exit(1);
  }

  // Build the project.
  std::filesystem::create_directories(cwd / fp / "src");
  std::filesystem::current_path(cwd / fp);
  auto comp_time_constants = spp::Map<spp::Str, spp::Str>();
  try {
    comp_time_constants = spp::cli::unit_test("rel", std::move(code));
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
