#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    TestMain,
    test_valid_main, R"(
    fun main(args: Vec[Str]) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    TestMain,
    test_valid_main_different_return_type, R"(
    fun main(args: Vec[Str]) -> Str { ret Str::from("0") }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    TestMain,
    test_valid_main_aliased_types, R"(
    fun main(args: Vec[Str]) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC_NO_MAIN(
    TestMain,
    test_invalid_main_missing,
    SppMissingMainFunctionError, R"(
    # No main function defined.
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC_NO_MAIN(
    TestMain,
    test_invalid_main_no_argument,
    SppMissingMainFunctionError, R"(
    fun main() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC_NO_MAIN(
    TestMain,
    test_invalid_main_argument_type_mismatch,
    SppMissingMainFunctionError, R"(
    fun main(args: Vec[std::bignum::bigint::BigInt]) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC_NO_MAIN(
    TestMain,
    test_invalid_main_borrow_convention,
    SppMissingMainFunctionError, R"(
    fun main(args: &Vec[Str]) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC_NO_MAIN(
    TestMain,
    test_invalid_main_extra_required_parameter,
    SppMissingMainFunctionError, R"(
    fun main(args: Vec[Str], x: S32) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC_NO_MAIN(
    TestMain,
    test_invalid_main_generic,
    SppMissingMainFunctionError, R"(
    fun main[T](args: Vec[Str]) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    TestMain,
    test_valid_main_optional_extra_parameter, R"(
    fun main(args: Vec[Str], x: S32 = 0) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    TestMain,
    test_valid_main_aliased_argument_type, R"(
    type StrVec = Vec[Str]
    fun main(args: StrVec) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    TestMain,
    test_valid_main_never_return_type, R"(
    fun main(args: Vec[Str]) -> ! {
        loop true { }
    }
)");
