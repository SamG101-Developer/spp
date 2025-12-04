#include "../test_macros.hpp"
import testex;


SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    TestMain,
    test_valid_main, R"(
    fun main(args: std::vector::Vec[std::string::Str]) -> std::void::Void { }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    TestMain,
    test_valid_main_different_return_type, R"(
    fun main(args: std::vector::Vec[std::string::Str]) -> std::string::Str { ret "0" }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    TestMain,
    test_valid_main_aliased_types, R"(
    use std::vector::Vec
    use std::string::Str
    use std::void::Void
    fun main(args: Vec[Str]) -> Void { }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC_NO_MAIN(
    TestMain,
    test_invalid_main_missing,
    SppMissingMainFunctionError, R"(
    # No main function defined.
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC_NO_MAIN(
    TestMain,
    test_invalid_main_no_argument,
    SppMissingMainFunctionError, R"(
    fun main() -> std::void::Void { }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC_NO_MAIN(
    TestMain,
    test_invalid_main_argument_type_mismatch,
    SppMissingMainFunctionError, R"(
    fun main(args: std::vector::Vec[std::bignum::bigint::BigInt]) -> std::void::Void { }
)");;
