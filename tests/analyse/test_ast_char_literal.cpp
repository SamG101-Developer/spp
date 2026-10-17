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

// FIXED
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CharLiteralAst,
  test_invalid_char_literal_with_two_characters,
  SppCharLiteralLengthError, R"(
    fun f() -> Void {
        let c = 'ab'
    }
)");

// "\x" escapes were not decoded at all: "b'\x0b'" read as the letter "x", which std's "is_whitespace" relies on.
// FIXED
SPP_TEST_SHOULD_PASS_SEMANTIC(
  CharLiteralAst,
  test_valid_byte_literal_with_a_hex_escape, R"(
    fun f() -> Void {
        let x = b'\x0b'
    }
)");

// FIXED
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CharLiteralAst,
  test_invalid_byte_literal_with_a_hex_escape_and_a_trailing_character,
  SppCharLiteralLengthError, R"(
    fun f() -> Void {
        let x = b'\x0bb'
    }
)");
