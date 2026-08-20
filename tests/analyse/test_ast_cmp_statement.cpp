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
