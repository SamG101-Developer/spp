#pragma once

import spp.cli;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.lex.lexer;
import spp.parse.errors.parser_error;
import spp.parse.parser_spp;
import std;


inline auto build_temp_project(std::string code, const bool add_main = true) -> void {
    auto cwd = std::filesystem::current_path();
    auto fp = "../../tests/test_outputs";

    std::cout << (cwd / fp).string() << std::endl;

    if (add_main) {
        code = "fun main(args: std::vector::Vec[std::string::Str]) -> std::void::Void { }\n" + code;
    }

    // Build the temporary project directory if it doesn't exist.
    if (not std::filesystem::exists(cwd / fp)) {
        std::filesystem::create_directory(cwd / fp);
        std::filesystem::current_path(cwd / fp);
        spp::cli::handle_init();
        spp::cli::handle_vcs();
        std::filesystem::current_path(cwd);
    }

    // Write the code to "cwd / fp / src / main.spp".
    std::filesystem::create_directories(cwd / fp / "src");
    std::ofstream ofs(cwd / fp / "src" / "main.spp");
    ofs << code;
    ofs.close();

    // Build the project.
    std::filesystem::current_path(cwd / fp);
    try {
        spp::cli::handle_build("rel", true);
    }
    catch (const spp::analyse::errors::SemanticError &e) {
        std::cout << e.what() << std::endl;
        spp::analyse::scopes::ScopeManager::cleanup();
        throw;
    }
    catch (const spp::parse::errors::SppSyntaxError &e) {
        std::cout << e.what() << std::endl;
        spp::analyse::scopes::ScopeManager::cleanup();
        throw;
    }
    catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
        spp::analyse::scopes::ScopeManager::cleanup();
        throw;
    }
    std::filesystem::current_path(cwd);
}
