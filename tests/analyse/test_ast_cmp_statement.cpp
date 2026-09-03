#include "../test_macros.hpp"

// Todo:
//  - test cmp functions
//  - test namespaced constants

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CmpStatementAst,
  test_invalid_type_mismatch,
  SppTypeMismatchError, R"(
    cmp x: S32 = false
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_value_literal, R"(
    cmp x: S32 = 1
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_value_strview_borrow, R"(
    cmp x: &StrView = "hello"
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_value_comp_identifier_copyanle, R"(
    cmp x: S32 = 1
    cmp y: S32 = x
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CmpStatementAst,
  test_invalid_value_comp_identifier_noncopyanle,
  SppMovingComptimeConstantMemoryError, R"(
    cls MyClass {
        !public x: Bool
    }
    cmp x: MyClass = MyClass(x=false)
    cmp y: MyClass = x
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CmpStatementAst,
  test_invalid_duplicate_name,
  SppIdentifierDuplicateError, R"(
    cmp x: S32 = 1
    cmp x: S32 = 2
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_value_binary_expression, R"(
    cmp x: S32 = 1 | 2
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_value_referencing_another_cmp_in_expression, R"(
    cmp x: S32 = 1
    cmp y: S32 = x + 1
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_value_array_literal, R"(
    cmp x: Arr[S32, 3_uz] = [1, 2, 3]
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_value_tuple_literal, R"(
    cmp x: (S32, S32) = (1, 2)
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_visibility_annotation, R"(
    !public cmp x: S32 = 1
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CmpStatementAst,
  test_invalid_circular_dependency,
  SppCompileTimeConstantError, R"(
    cmp x: S32 = y + 1
    cmp y: S32 = x + 1
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CmpStatementAst,
  test_invalid_self_referential_dependency,
  SppCompileTimeConstantError, R"(
    cmp x: S32 = x + 1
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CmpStatementAst,
  test_invalid_circular_dependency_three_way,
  SppCompileTimeConstantError, R"(
    cmp a: S32 = b + 1
    cmp b: S32 = c + 1
    cmp c: S32 = a + 1
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CmpStatementAst,
  test_invalid_circular_dependency_through_array_element,
  SppCompileTimeConstantError, R"(
    cmp arr: Arr[S32, 2_uz] = [b, 2]
    cmp b: S32 = arr.0
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CmpStatementAst,
  test_invalid_circular_dependency_through_tuple_element,
  SppCompileTimeConstantError, R"(
    cmp tup: (S32, S32) = (b, 2)
    cmp b: S32 = tup.0
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_value_chain_of_cmp_references, R"(
    cmp a: S32 = 1
    cmp b: S32 = a + 1
    cmp c: S32 = b + a
    cmp d: S32 = c * b
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_value_array_literal_referencing_another_cmp, R"(
    cmp a: S32 = 7
    cmp arr: Arr[S32, 2_uz] = [a, a]
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_value_tuple_literal_referencing_another_cmp, R"(
    cmp a: S32 = 7
    cmp tup: (S32, S32) = (a, 2)
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_value_case_of_literal_patterns, R"(
    cmp a: S32 = case 1 of {
        == 1 { 10 }
        == 2 { 20 }
        else { 30 }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_value_case_followed_by_another_cmp, R"(
    cmp a: S32 = case 1 of {
        == 1 { 10 }
        else { 30 }
    }

    cmp b: S32 = 2
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_value_case_followed_by_function, R"(
    cmp a: S32 = case 1 of {
        == 1 { 10 }
        else { 30 }
    }

    fun f() -> S32 {
        ret 1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_value_case_followed_by_class_and_sup, R"(
    cmp a: S32 = case 1 of {
        == 1 { 10 }
        else { 30 }
    }

    cls MyClass {
        !public x: S32
    }

    sup MyClass {
        !public fun f(&self) -> S32 { ret 1 }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_value_two_case_values_in_a_row, R"(
    cmp a: S32 = case 1 of {
        == 1 { 10 }
        else { 30 }
    }

    cmp b: S32 = case 2 of {
        == 2 { 20 }
        else { 40 }
    }

    fun f() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_value_nested_case, R"(
    cmp a: S32 = case 1 of {
        == 1 {
            case 2 of {
                == 2 { 20 }
                else { 30 }
            }
        }
        else { 40 }
    }

    fun f() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_value_case_referencing_another_cmp, R"(
    cmp a: S32 = 1

    cmp b: S32 = case a of {
        == 1 { 10 }
        else { 30 }
    }

    fun f() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_value_case_of_tuple_destructure, R"(
    cmp a: S32 = case (1, 5) of {
        is (1, y) { y }
        else { 0 }
    }

    fun f() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CmpStatementAst,
  test_valid_value_case_in_sup_block, R"(
    cls MyClass { }

    sup MyClass {
        cmp a: S32 = case 1 of {
            == 1 { 10 }
            else { 30 }
        }

        !public fun f(&self) -> S32 { ret 1 }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CmpStatementAst,
  test_invalid_value_case_type_mismatch,
  SppTypeMismatchError, R"(
    cmp a: S32 = case 1 of {
        == 1 { false }
        else { true }
    }
)");
