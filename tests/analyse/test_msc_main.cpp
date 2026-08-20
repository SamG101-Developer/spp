#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    TestMain,
    test_valid_main, R"(
    fun main() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    TestMain,
    test_valid_main_different_return_type, R"(
    fun main() -> Str { ret Str::from("0") }
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
    fun main(a: Vec[Str]) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC_NO_MAIN(
    TestMain,
    test_invalid_main_generic,
    SppMissingMainFunctionError, R"(
    fun main[T]() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    TestMain,
    test_valid_main_optional_extra_parameter, R"(
    fun main(x: S32 = 0) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    TestMain,
    test_valid_main_never_return_type, R"(
    fun main() -> ! {
        loop true { }
    }
)");
