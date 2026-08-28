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

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_pattern_guard_with_variant_narrowing, R"(
    fun f(p: Opt[Str]) -> Void {
        case p of {
            is Some[Str](val) and val == Str::from("x") { }
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
        let x = case 1 == 1 {
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

// A branch that terminates never produces a value, so it takes no part in unifying the branch types. These cover the
// filtering in "ValidateInconsistentTypes" - both that a terminating branch is ignored, and that everything else about
// it is still checked.

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_ret_in_else_branch_assigned, R"(
    fun f() -> Void {
        let x = case true { 1 } else { ret }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_ret_in_first_branch_assigned, R"(
    fun f() -> Void {
        let x = case true { ret } else { 1 }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_ret_in_every_branch_unassigned, R"(
    fun f() -> Void {
        case true { ret } else { ret }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_ret_in_else_branch_assigned_pattern_match, R"(
    fun f(o: Opt[S32]) -> S32 {
        let x = case o of {
            is Some[S32](val) { val }
            else { ret 0 }
        }
        ret x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_ret_in_else_branch_propagating_residual, R"(
    cls Err1 { }

    fun f() -> Res[S32, Err1] {
        let x = case true { 1 } else { ret Fail(err=Err1()) }
        ret Pass(val=x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_ret_in_else_branch_assigned_explicit_type, R"(
    fun f() -> Void {
        let x: S32 = case true { 1 } else { ret }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CaseExpressionAst,
    test_invalid_branch_types_still_checked_when_none_terminate,
    SppTypeMismatchError, R"(
    fun f() -> Void {
        let x = case true { 1 } else { false }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CaseExpressionAst,
    test_invalid_branch_types_still_checked_beside_a_terminating_branch,
    SppTypeMismatchError, R"(
    fun f() -> Void {
        let x = case 1 of {
            == 1 { 1 }
            == 2 { false }
            else { ret }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CaseExpressionAst,
    test_invalid_surviving_branch_type_against_explicit_type,
    SppTypeMismatchError, R"(
    fun f() -> Void {
        let x: Bool = case true { 1 } else { ret }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CaseExpressionAst,
    test_invalid_ret_value_in_branch_still_checked,
    SppTypeMismatchError, R"(
    fun f() -> Bool {
        let x = case true { 1 } else { ret 123 }
        ret true
    }
)");
