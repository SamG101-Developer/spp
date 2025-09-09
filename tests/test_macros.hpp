#pragma once
#include <spp/lex/lexer.hpp>
#include <spp/parse/parser_spp.hpp>


#define SPP_TEST_SHOULD_PASS_SYNTACTIC(what, code) \
    TEST(SppParser, what) { \
        auto ast = INJECT_CODE(code, parse); \
        std::cout << "AST: " << ast->operator std::string() << std::endl; \
    }


#define SPP_TEST_SHOULD_FAIL_SYNTACTIC(what, code) \
    TEST(SppParser, what) { \
        EXPECT_THROW({ \
            INJECT_CODE(code, parse); \
        }, spp::parse::errors::SppSyntaxError); \
    }
