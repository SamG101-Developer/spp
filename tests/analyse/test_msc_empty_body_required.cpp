#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  EmptyBodyRequired,
  test_invalid_ffi_function_with_a_body,
  SppEmptyBodyRequiredError, R"(
    !ffi(symbol="zz_absent")
    fun g(a: S32) -> S32 {
        ret a
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  EmptyBodyRequired,
  test_invalid_ffi_function_with_a_non_returning_body,
  SppEmptyBodyRequiredError, R"(
    !ffi(symbol="zz_absent")
    fun g(a: S32) -> Void {
        let b = a
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  EmptyBodyRequired,
  test_invalid_abstract_method_with_a_body,
  SppEmptyBodyRequiredError, R"(
    cls A { }

    sup A {
        !abstract_method
        fun f(&self) -> Void {
            let x = 1
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  EmptyBodyRequired,
  test_invalid_abstract_method_with_a_returning_body,
  SppEmptyBodyRequiredError, R"(
    cls A { }

    sup A {
        !abstract_method
        fun f(&self) -> S32 {
            ret 1
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  EmptyBodyRequired,
  test_invalid_zero_type_class_with_an_attribute,
  SppEmptyBodyRequiredError, R"(
    !zero_type
    cls Marker {
        x: S32
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  EmptyBodyRequired,
  test_invalid_zero_type_class_with_multiple_attributes,
  SppEmptyBodyRequiredError, R"(
    !zero_type
    cls Marker {
        x: S32
        y: S32
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  EmptyBodyRequired,
  test_invalid_zero_type_class_with_a_zero_sized_attribute,
  SppEmptyBodyRequiredError, R"(
    !zero_type
    cls Inner { }

    !zero_type
    cls Marker {
        x: Inner
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  EmptyBodyRequired,
  test_valid_ffi_function_with_an_empty_body, R"(
    !ffi(symbol="zz_absent")
    fun g(a: S32) -> S32 { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  EmptyBodyRequired,
  test_valid_abstract_method_with_an_empty_body, R"(
    cls A { }

    sup A {
        !abstract_method
        fun f(&self) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  EmptyBodyRequired,
  test_valid_abstract_method_with_a_comment_only_body, R"(
    cls A { }

    sup A {
        !abstract_method
        fun f(&self) -> S32 {
            # Overriders return the number of things.
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  EmptyBodyRequired,
  test_valid_zero_type_class_with_an_empty_body, R"(
    !zero_type
    cls Marker { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  EmptyBodyRequired,
  test_valid_zero_type_class_with_a_comment_only_body, R"(
    !zero_type
    cls Marker {
        # Nothing to store; the type is the whole signal.
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  EmptyBodyRequired,
  test_valid_native_function_with_a_body, R"(
    fun g(a: S32) -> S32 {
        ret a
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  EmptyBodyRequired,
  test_valid_non_zero_type_class_with_attributes, R"(
    cls Point {
        x: S32
        y: S32
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  EmptyBodyRequired,
  test_valid_virtual_method_with_a_body, R"(
    cls A { }

    sup A {
        !virtual_method
        fun f(&self) -> S32 {
            ret 1
        }
    }
)");
