#pragma once
#include <spp/cli.hpp>
#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/lex/lexer.hpp>
#include <spp/parse/parser_spp.hpp>

#include <gtest/gtest.h>


inline auto build_temp_project(std::string code, const bool add_main = true) {
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
    spp::cli::handle_build("rel", true);
    std::filesystem::current_path(cwd);
}


#define SPP_TEST_SHOULD_PASS_SYNTACTIC(name, code)                        \
    TEST(SppParser, name) {                                               \
        auto ast = INJECT_CODE(code, parse);                              \
        std::cout << "AST: " << ast->operator std::string() << std::endl; \
    }


#define SPP_TEST_SHOULD_FAIL_SYNTACTIC(name, code) \
    TEST(SppParser, name) {                        \
        EXPECT_THROW({                             \
            INJECT_CODE(code, parse);              \
        }, spp::parse::errors::SppSyntaxError);    \
    }


#define SPP_TEST_SHOULD_PASS_SEMANTIC(name, code) \
    TEST(SppAnalyse, name) {                      \
        build_temp_project(code);                 \
    }


#define SPP_TEST_SHOULD_FAIL_SEMANTIC(name, error, code)     \
    TEST(SppAnalyse, name) {                                 \
        EXPECT_THROW({                                       \
            try {                                            \
                build_temp_project(code);                    \
            } catch (const spp::analyse::errors::error &e) { \
                std::cout << e.what() << std::endl;          \
                throw;                                       \
            }                                                \
        }, spp::analyse::errors::error);                     \
    }
