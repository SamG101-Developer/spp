#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CaseExpressionAst,
    test_invalid_case_expression,
    SppExpressionTypeInvalidError, R"(
    fun f() -> std::void::Void {
        case std::boolean::Bool == 1 { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CaseExpressionAst,
    test_invalid_multiple_destructures_on_single_branch,
    SppCaseBranchMultipleDestructuresError, R"(
    fun f() -> std::void::Void {
        case (1, 2) of {
            is (a, b), (a, 1) { }
        }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CaseExpressionAst,
    test_invalid_else_branch_not_last,
    SppCaseBranchElseNotLastError, R"(
    fun f() -> std::void::Void {
        case 1 of {
            else { }
            == 2 { }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_simple_comparison, R"(
    fun f() -> std::void::Void {
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
    fun f() -> std::void::Void {
        case [1, 2, 3] of {
            is [1, a, b] { }
            is [2, c, d] { }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_simple_tuple_destructure, R"(
    fun f() -> std::void::Void {
        case (1, 2, 3) of {
            is (1, a, b) { }
            is (2, c, d) { }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_case_else_case, R"(
    fun f(a: std::number::S32, b: std::number::S32) -> std::void::Void {
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


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CaseExpressionAst,
    test_valid_partial_move_in_pattern, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    fun f(p: Point) -> std::void::Void {
        let x = case p of {
            is Point(x, y=10) { x }
            is Point(x=10, y) { y }
            else { 0 }
        }
    }
)");
