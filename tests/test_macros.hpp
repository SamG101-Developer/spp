#pragma once
#include <spp/macros.hpp>
#include <spp/parse/macros.hpp>
#include <testex/macros.hpp>

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


#define SPP_TEST_SHOULD_PASS_SYNTACTIC(name, code)                        \
    TESTEX_TEST(SppParser, name) {                                        \
        auto ast = INJECT_CODE(code, parse);                              \
        std::cout << "AST: " << ast->operator std::string() << std::endl; \
    }


#define SPP_TEST_SHOULD_FAIL_SYNTACTIC(name, code)                            \
    TESTEX_TEST(SppParser, name) {                                            \
        auto throwable = [&]() { INJECT_CODE(code, parse); };                 \
        testex::assert_throws<spp::parse::errors::SppSyntaxError>(throwable); \
    }


#define SPP_TEST_SHOULD_PASS_SEMANTIC(group, name, code) \
    TESTEX_TEST(group, name) {                           \
        build_temp_project(code);                        \
    }


#define SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(group, name, code) \
    TESTEX_TEST(group, name) {                                   \
        build_temp_project(code, false);                         \
    }


#define SPP_TEST_SHOULD_FAIL_SEMANTIC(group, name, error, code)        \
    TESTEX_TEST(group, name) {                                         \
        auto throwable = [&]() { build_temp_project(code); };          \
        testex::assert_throws<spp::analyse::errors::error>(throwable); \
    }


#define SPP_TEST_SHOULD_FAIL_SEMANTIC_NO_MAIN(group, name, error, code) \
    TESTEX_TEST(group, name) {                                          \
        auto throwable = [&]() { build_temp_project(code, false); };    \
        testex::assert_throws<spp::analyse::errors::error>(throwable);  \
    }
