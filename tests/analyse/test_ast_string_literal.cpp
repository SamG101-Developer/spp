#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    StringLiteralAst,
    test_valid_string_literal, R"(
    fun f() -> Void {
        let x = "hello"
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    StringLiteralAst,
    test_valid_string_literal_with_escapes, R"(
    fun f() -> Void {
        let x = "hello\nworld\t!"
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    StringLiteralAst,
    test_valid_string_literal_multi_byte_unicode, R"(
    fun f() -> Void {
        let x = "héllo wörld €"
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    StringLiteralAst,
    test_valid_byte_prefixed_string_literal, R"(
    fun f() -> Void {
        let x = b"hello"
    }
)");
