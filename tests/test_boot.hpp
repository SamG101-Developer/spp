#pragma once

import spp.cli;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.lex.lexer;
import spp.parse.errors.parser_error;
import spp.parse.parser_spp;
import sys;
import std;


inline auto build_temp_project(std::string code, const bool add_main = true) -> void {
    const auto cwd = std::filesystem::current_path();
    const auto fp = "../../tests/test_outputs";

    if (add_main) {
        code = "fun main(args: Vec[Str]) -> Void { }\n" + code;
    }

    // Ensure the output directory exists before opening the lock file inside it.
    std::filesystem::create_directories(cwd / fp);

    // Acquire the same cross-process file lock that ModuleTree uses, so that
    // initialization (handle_init + handle_vcs) is serialized across parallel
    // test workers. Use spp.toml as the sentinel: it is written by handle_init,
    // so its absence means initialization has not completed.
    const auto lock_path = (cwd / fp / ".lock").string();
    const int init_lock_fd = sys::open(lock_path.c_str(), sys::O_RDWR | sys::O_CREAT);
    sys::flock(init_lock_fd, sys::LOCK_EX);

    if (not std::filesystem::exists(cwd / fp / "spp.toml")) {
        std::filesystem::current_path(cwd / fp);
        spp::cli::handle_init();
        spp::cli::handle_vcs();
        std::filesystem::current_path(cwd);
    }

    sys::flock(init_lock_fd, sys::LOCK_UN);
    sys::close(init_lock_fd);

    // Build the project.
    std::filesystem::create_directories(cwd / fp / "src");
    std::filesystem::current_path(cwd / fp);
    try {
        spp::cli::unit_test("rel", std::move(code));
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
}
