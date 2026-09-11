#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClassAttributeAst,
  test_invalid_duplicate_identifier,
  SppIdentifierDuplicateError, R"(
    cls A {
        a: Str
        a: Str
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClassAttributeAst,
  test_invalid_convention_mut,
  SppSecondClassBorrowViolationError, R"(
    cls A {
        a: &mut Str
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClassAttributeAst,
  test_invalid_convention_ref,
  SppSecondClassBorrowViolationError, R"(
    cls A {
        a: &Str
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClassAttributeAst,
  test_valid_from_generic_substitution, R"(
    cls A[T] {
        a: T
    }

    fun f() -> Void {
        let a = A[Str]()
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClassAttributeAst,
  test_invalid_convention_mut_from_generic_substitution,
  SppSecondClassBorrowViolationError, R"(
    cls A[T] {
        a: T
    }

    fun f() -> Void {
        let a = A[&mut Str]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClassAttributeAst,
  test_invalid_convention_ref_from_generic_substitution,
  SppSecondClassBorrowViolationError, R"(
    cls A[T] {
        a: T
    }

    fun f() -> Void {
        let a = A[&Str]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClassAttributeAst,
  test_valid_attributes, R"(
    cls A {
        a: Str
        b: Str
    }

    cls B {
        a: Str
        b: Str
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClassAttributeAst,
  test_valid_attributes_with_super_class, R"(
    cls A {
        a: Str
    }

    cls B {
        b: Str
    }

    cls C { }
    sup C ext A {}
    sup C ext B {}
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClassAttributeAst,
  test_invalid_default_value,
  SppTypeMismatchError, R"(
    cls A {
        a: Str = 1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClassAttributeAst,
  test_valid_default_value, R"(
    cls A {
        a: Bool = false
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClassAttributeAst,
  test_invalid_unknown_type,
  SppIdentifierUnknownError, R"(
    cls A {
        a: Unknown
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClassAttributeAst,
  test_invalid_unknown_namespaced_type,
  SppIdentifierUnknownError, R"(
    cls A {
        a: std::Unknown
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClassAttributeAst,
  test_valid_visibility_annotations, R"(
    cls A {
        !public a: Str
        !private b: Str
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClassAttributeAst,
  test_valid_default_value_function_call, R"(
    cls A {
        a: Str = Str::from("Hello")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClassAttributeAst,
  test_valid_default_value_variant, R"(
    cls A {
        a: Opt[Str] = None
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClassAttributeAst,
  test_invalid_default_value_variant_not_member,
  SppTypeMismatchError, R"(
    cls A {
        a: Opt[Str] = true
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClassAttributeAst,
  test_invalid_case_default,
  SppInvalidDefaultValueError, R"(
    cls A {
        a: S32 = case true { 1_s32 } else { 2_s32 }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClassAttributeAst,
  test_invalid_ret_in_a_scope_default,
  SppInvalidDefaultValueError, R"(
    cls A {
        a: S32 = { ret 1_s32 }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClassAttributeAst,
  test_invalid_scope_default,
  SppInvalidDefaultValueError, R"(
    cls A {
        a: S32 = {
            let x = 1_s32
            x + 1_s32
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClassAttributeAst,
  test_invalid_closure_default,
  SppInvalidDefaultValueError, R"(
    cls A {
        a: FunRef[(S32,), S32] = (x: S32) { ret x }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClassAttributeAst,
  test_invalid_scope_default_on_a_constructed_class,
  SppInvalidDefaultValueError, R"(
    fun f() -> Void {
        let y = A()
    }

    cls A {
        !public
        a: S32 = {
            let x = 1_s32
            x
        }
    }
)");
