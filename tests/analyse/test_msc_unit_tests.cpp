#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUnitTestAnnotation,
    test_invalid_unit_test_with_parameters,
    SppUnitTestInvalidSignatureError, R"(
    !test
    fun test_a(x: S32) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUnitTestAnnotation,
    test_invalid_unit_test_with_non_void_return,
    SppUnitTestInvalidSignatureError, R"(
    !test
    fun test_a() -> S32 { ret 1 }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUnitTestAnnotation,
    test_invalid_unit_test_with_generic_parameters,
    SppUnitTestInvalidSignatureError, R"(
    !test
    fun test_a[T]() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUnitTestAnnotation,
    test_invalid_unit_test_as_cmp_function,
    SppUnitTestInvalidSignatureError, R"(
    !test
    cmp fun test_a() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUnitTestAnnotation,
    test_invalid_call_to_unit_test,
    SppUnitTestNotCallableError, R"(
    !test
    fun test_a() -> Void { }

    fun caller() -> Void {
        test_a()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestUnitTestAnnotation,
    test_valid_unit_test_signature, R"(
    !test
    fun test_a() -> Void { }

    !test(group="explicit")
    fun test_b() -> Void { }
)");
