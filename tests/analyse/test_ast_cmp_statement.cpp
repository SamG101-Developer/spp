#include "../test_macros.hpp"

// Todo:
//  - test cmp functions
//  - test namespaced constants

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CmpStatementAst,
    test_invalid_type_mismatch,
    SppTypeMismatchError, R"(
    cmp x: std::number::S32 = false
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CmpStatementAst,
    test_invalid_type_convention_mut,
    SppSecondClassBorrowViolationError, R"(
    cmp x: &mut std::number::S32 = 1
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CmpStatementAst,
    test_invalid_type_convention_ref,
    SppSecondClassBorrowViolationError, R"(
    cmp x: &std::number::S32 = 1
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CmpStatementAst,
    test_valid_value_literal, R"(
    cmp x: std::number::S32 = 1
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CmpStatementAst,
    test_valid_value_comp_identifier_copyanle, R"(
    cmp x: std::number::S32 = 1
    cmp y: std::number::S32 = x
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CmpStatementAst,
    test_invalid_value_comp_identifier_noncopyanle,
    SppMoveFromPinnedMemoryError, R"(
    cls MyClass {
        x: std::string_view::StrView
    }
    cmp x: MyClass = MyClass(x="hello")
    cmp y: MyClass = x
)");
