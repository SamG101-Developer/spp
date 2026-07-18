#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CaseExpressionAst,
    test_invalid_case_expression,
    SppInvalidPrimaryExpressionError, R"(
    fun f() -> Void {
        case Bool == 1 { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CaseExpressionAst,
    test_invalid_else_branch_not_last,
    SppCaseBranchElseNotLastError, R"(
    fun f() -> Void {
        case 1 of {
            else { }
            == 2 { }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_simple_comparison, R"(
    fun f() -> Void {
        case 1 of {
            == 1 { }
            == 2 { }
            else { }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_simple_array_destructure, R"(
    fun f() -> Void {
        case [1, 2, 3] of {
            is [1, a, b] { }
            is [2, c, d] { }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_simple_tuple_destructure, R"(
    fun f() -> Void {
        case (1, 2, 3) of {
            is (1, a, b) { }
            is (2, c, d) { }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_case_else_case, R"(
    fun f(a: S32, b: S32) -> Void {
        let x = case a == 1 {
            "hello world"
        }
        else case b == 2 {
            "goodbye world"
        }
        else {
            "neither"
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CaseExpressionAst,
    test_invalid_case_else_case,
    SppTypeMismatchError, R"(
    fun f(a: S32, b: S32) -> Void {
        let x = case a == 1 {
            "hello world"
        }
        else case b == 2 {
            123
        }
        else {
            false
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_comparison_multiple_values, R"(
    fun f() -> Void {
        case 1 of {
            == 1, 2, 3 { }
            else { }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_comparison_ne, R"(
    fun f() -> Void {
        case 1 of {
            != 2 { }
            else { }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_comparison_lt, R"(
    fun f() -> Void {
        case 1 of {
            < 2 { }
            else { }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_comparison_le, R"(
    fun f() -> Void {
        case 1 of {
            <= 2 { }
            else { }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_comparison_gt, R"(
    fun f() -> Void {
        case 1 of {
            > 2 { }
            else { }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_comparison_ge, R"(
    fun f() -> Void {
        case 1 of {
            >= 2 { }
            else { }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_pattern_guard, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f(p: Point) -> Void {
        case p of {
            is Point(x, y) and x == 1 { }
            else { }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CaseExpressionAst,
    test_invalid_pattern_guard_non_boolean,
    SppExpressionNotBooleanError, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f(p: Point) -> Void {
        case p of {
            is Point(x, y) and x { }
            else { }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CaseExpressionAst,
    test_invalid_missing_else_as_expression,
    SppCaseBranchMissingElseError, R"(
    fun f() -> Void {
        let x = case 1 of {
            == 1 { 1 }
            == 2 { 2 }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CaseExpressionAst,
    test_invalid_branch_type_mismatch,
    SppTypeMismatchError, R"(
    fun f() -> Void {
        let x = case 1 of {
            == 1 { 1 }
            else { "not a number" }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_of_form_else_case, R"(
    fun f() -> Void {
        case 1 == 1 {
            "hello world"
        }
        else case 2 == 2 {
            "goodbye world"
        }
        else {
            "neither"
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_partial_move_in_pattern, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f(p: Point) -> Void {
        let x = case p of {
            is Point(x, y=10) { x }
            is Point(x=10, y) { y }
            else { 0 }
        }
    }
)");
