#pragma once
#include <spp/lex/lexer.hpp>
#include <spp/parse/parser_spp.hpp>


#define SPP_TEST_SHOULD_PASS_SYNTACTIC(what, code) \
    TEST(SppParser, what) { \
        INJECT_CODE(code, parse); \
    }
