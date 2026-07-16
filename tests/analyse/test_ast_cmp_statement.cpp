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
        !public x: StrView
    }
    cmp x: MyClass = MyClass(x="hello")
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
