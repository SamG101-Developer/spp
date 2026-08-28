#pragma once

// The syntactic macros, split out of "test_macros.hpp" because they are the only thing that needs the lexer and the
// parser, and only one test file uses them. Importing them from the shared header made every semantic test build the
// parser as well.
#include <spp/parse/macros.hpp>
#include "test_macros.hpp"

import spp.lex.lexer;
import spp.parse.parser_spp;
import spp.parse.errors.parser_error;

#define SPP_TEST_SHOULD_PASS_SYNTACTIC(name, code) \
    TEST(SppParser, name) {                        \
        auto ast = INJECT_CODE(code, parse);       \
    }

#define SPP_TEST_SHOULD_FAIL_SYNTACTIC(name, code)                                  \
    TEST(SppParser, name) {                                                         \
        EXPECT_THROW(INJECT_CODE(code, parse), spp::parse::errors::SppSyntaxError); \
    }
