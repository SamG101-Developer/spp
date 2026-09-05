#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestPrelude,
  test_valid_zero_type_annotation_without_an_import, R"(
    !zero_type
    cls Marker { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestPrelude,
  test_valid_abstract_method_annotation_without_an_import, R"(
    cls A { }

    sup A {
        !abstract_method
        fun f(&self) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestPrelude,
  test_valid_virtual_method_annotation_without_an_import, R"(
    cls A { }

    sup A {
        !virtual_method
        fun f(&self) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestPrelude,
  test_valid_ffi_annotation_without_an_import, R"(
    !ffi(symbol="zz_absent")
    fun g(a: S32) -> S32 { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestPrelude,
  test_valid_visibility_annotations_without_an_import, R"(
    !public
    cls A { }

    !private
    cls B { }

    !protected
    cls C { }

    !package
    cls D { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestPrelude,
  test_valid_test_annotation_without_an_import, R"(
    !test
    fun a_unit_test() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestPrelude,
  test_valid_prelude_types_without_an_import, R"(
    fun f(a: Vec[S32], b: Opt[Str], c: Bool) -> Void {
        std::mem::ops::drop(a)
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestPrelude,
  test_invalid_reimport_of_a_prelude_annotation_zero_type,
  SppIdentifierDuplicateError, R"(
    use std::annotations::zero_type

    !zero_type
    cls Marker { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestPrelude,
  test_invalid_reimport_of_a_prelude_annotation_abstract_method,
  SppIdentifierDuplicateError, R"(
    use std::annotations::abstract_method
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestPrelude,
  test_invalid_reimport_of_a_prelude_type,
  SppIdentifierDuplicateError, R"(
    use std::vector::Vec
)");
