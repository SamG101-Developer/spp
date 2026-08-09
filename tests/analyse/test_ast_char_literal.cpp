#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CharLiteralAst,
    test_valid_char_literal, R"(
    fun f() -> Void {
        let x = 'a'
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CharLiteralAst,
    test_valid_char_literal_multi_byte_unicode, R"(
    fun f() -> Void {
        let x = '€'
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CharLiteralAst,
    test_valid_char_literal_escape, R"(
    fun f() -> Void {
        let x = '\n'
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CharLiteralAst,
    test_valid_byte_prefixed_char_literal, R"(
    fun f() -> Void {
        let x = b'a'
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CharLiteralAst,
    test_invalid_byte_prefixed_char_literal_multi_byte_unicode,
    SppCharLiteralOutOfBoundsError, R"(
    fun f() -> Void {
        let x = b'€'
    }
)");
