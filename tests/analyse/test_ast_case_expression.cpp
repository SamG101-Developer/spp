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
            else { std::mem::ops::drop(p) }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_pattern_guard_with_variant_narrowing, R"(
    fun f(p: Opt[Str]) -> Void {
        let x = Str::from("x")
        case p of {
            is Some[Str](val) and val == x { }
            else { std::mem::ops::drop(p) }
        }
        std::mem::ops::drop(x)
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
            else {
                std::mem::ops::drop(p)
                0
            }
        }
    }
)");

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

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_branch_type_mismatch_in_statement_position,
  SppTypeMismatchError, R"(
    fun f() -> Void {
        case 1 of {
            == 1 { 1 }
            else { "not a number" }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_branch_type_mismatch_in_statement_position_all_copy,
  SppTypeMismatchError, R"(
    fun f() -> Void {
        case 1 of {
            == 1 { 1 }
            else { true }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_branch_non_void_in_statement_position,
  SppDiscardedValueError, R"(
    fun f() -> Void {
        case 1 of {
            == 1 { 1 }
            else { 2 }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_branches_naming_functions_into_a_function_type, R"(
    fun add_one(x: S32) -> S32 { ret x + 1 }
    fun sub_one(x: S32) -> S32 { ret x - 1 }

    fun f(b: Bool) -> S32 {
        let g: std::function::FunRef[(S32,), S32] = case b { add_one } else { sub_one }
        ret g(10)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_case_value_passed_straight_to_a_call, R"(
    fun g(s: &StrView) -> Void { }
    fun h(n: S32) -> Void { }

    fun f(b: Bool) -> Void {
        g(case b { "x" } else { "y" })
        h(case b { 1 } else { 2 })
    }
)");

// A pattern guard runs before its branch is chosen, so a move it makes also happens on the path into the next
// branch when it answers false. Guards may not move.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_move_in_pattern_guard_then_use,
  SppPatternGuardMovesValueError, R"(
    cls GuardL { }
    cls GuardP { !public a: S32 }

    sup GuardP ext std::copy::Copy { }

    fun eat(l: GuardL) -> Bool {
        let GuardL() = l
        ret true
    }

    fun f(p: GuardP) -> Void {
        let l = GuardL()
        case p of {
            is GuardP(a) and eat(l) { }
            else { }
        }
        let GuardL() = l
    }
)");

// Re-binding the subject's own name in a nested pattern segfaulted in codegen (the inner "val" shadows the subject).
SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_nested_case_rebinding_the_subject_name, R"(
    fun f(x: Opt[Opt[S32]]) -> S32 {
        ret case x of {
            is Some[Opt[S32]](val) {
                case val of {
                    is Some[S32](val) { val }
                    else { 0 }
                }
            }
            else { 0 }
        }
    }
)");

// FIXED
SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_every_branch_returns_as_last_statement, R"(
    fun f(c: Bool) -> S32 {
        case c { ret 1 } else { ret 2 }
    }
)");

// FIXED
SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_every_of_branch_returns_as_last_statement, R"(
    fun f(x: S32) -> S32 {
        case x of {
            == 1 { ret 1 }
            else { ret 2 }
        }
    }
)");

// The condition of the non-"of" form was never checked to be a "Bool", and reached codegen as a non-"i1" branch.
// FIXED
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_non_boolean_condition_without_branches,
  SppExpressionNotBooleanError, R"(
    fun f(x: S32) -> Void {
        case x { }
    }
)");

// FIXED
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_non_boolean_condition_with_else,
  SppExpressionNotBooleanError, R"(
    fun f(x: S32) -> S32 {
        ret case x { 1 } else { 2 }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_short_pattern_form_yielding_a_non_boolean, R"(
    fun f(o: Opt[S32]) -> S32 {
        ret case o is Some[S32](..) { 1 } else { 0 }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_short_pattern_form_that_cannot_fail_needs_no_else, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f(p: Point) -> S32 {
        ret case p is Point(x, y) { x + y }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_short_pattern_form_that_can_fail_needs_an_else,
  SppCaseBranchMissingElseError, R"(
    fun f(o: Opt[S32]) -> S32 {
        ret case o is Some[S32](val) { val }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_else_after_short_pattern_form_that_cannot_fail,
  SppCaseBranchUnreachableError, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f(p: Point) -> S32 {
        ret case p is Point(x, y) { x } else { 0 }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_short_pattern_form_chained_with_else_case, R"(
    fun f(x: Opt[S32], y: Opt[S32]) -> S32 {
        ret case x is Some[S32](val) { val }
        else case y is Some[S32](val) { val }
        else { 0 }
    }
)");

// The short "case x is P(..)" form takes the subject on its match edge, as the "of" form does: moving the payload out
// moves "o" on that path, and the other path leaves it whole, so using it afterwards is a use of maybe-moved memory.
// FIXED
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_short_pattern_form_payload_moved_then_subject_used,
  SppInconsistentlyInitializedMemoryUseError, R"(
    fun f(o: Opt[Str]) -> Void {
        case o is Some[Str](val) { std::mem::ops::drop(val) }
        std::mem::ops::drop(o)
    }
)");

// FIXED
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_pattern_binds_the_same_name_twice,
  SppIdentifierDuplicateError, R"(
    cls DupP {
        !public x: S32
        !public y: S32
    }

    sup DupP ext std::copy::Copy { }

    fun f(p: DupP) -> Void {
        case p of {
            is DupP(x, y as x) { }
            else { }
        }
    }
)");

// A "case" with an "else", written as a statement in a loop body, was checked against the loop's own type (left in
// "AssignmentTargetType" for "exit" values) - a bogus E1, which crashed while being formatted.
// FIXED
SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_case_statement_with_else_in_an_infinite_loop, R"(
    fun f(c: Bool) -> Void {
        loop true {
            case c { } else { }
        }
    }
)");

// FIXED
SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_case_statement_with_else_in_a_returned_loop, R"(
    fun f(c: Bool) -> S32 {
        ret loop true {
            case c { } else { }
            exit 5
        }
    }
)");

// FIXED
SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_case_statement_with_else_before_a_blocks_value, R"(
    fun f(c: Bool) -> S32 {
        let x = {
            case c { } else { }
            5
        }
        ret x
    }
)");

// FIXED
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_move_in_pattern_guard_then_use_in_the_else_branch,
  SppPatternGuardMovesValueError, R"(
    cls GuardL2 { }
    cls GuardP2 { !public a: S32 }

    sup GuardP2 ext std::copy::Copy { }

    fun eat(l: GuardL2) -> Bool {
        let GuardL2() = l
        ret true
    }

    fun f(p: GuardP2) -> Void {
        let l = GuardL2()
        case p of {
            is GuardP2(a) and eat(l) { }
            else { let GuardL2() = l }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_pattern_guard_borrowing_a_value, R"(
    cls GuardL3 { }
    cls GuardP3 { !public a: S32 }

    sup GuardP3 ext std::copy::Copy { }

    fun peek(l: &GuardL3) -> Bool { ret true }

    fun f(p: GuardP3) -> Void {
        let l = GuardL3()
        case p of {
            is GuardP3(a) and peek(&l) { }
            else { }
        }
        let GuardL3() = l
    }
)");

// Exhaustiveness: a "case" that matches every value of its subject needs no "else" ("case_utils::CoverageOf"), and a
// branch after every value is matched can never run, so it is an error.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_exhaustive_variant_case_as_an_expression_without_else, R"(
    fun f(x: Opt[S32]) -> S32 {
        let n = case x of {
            is Some[S32](val) { val }
            is None() { 0 }
        }
        ret n
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_variant_case_missing_a_member_without_else,
  SppCaseBranchMissingElseError, R"(
    cls A { }
    cls B { }
    cls C { }

    fun f(x: A or B or C) -> S32 {
        let n = case x of {
            is A() { 1 }
            is B() { 2 }
        }
        ret n
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_guarded_branch_does_not_cover,
  SppCaseBranchMissingElseError, R"(
    fun f(x: Opt[S32]) -> S32 {
        let n = case x of {
            is Some[S32](val) and val > 0 { val }
            is None() { 0 }
        }
        ret n
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_bool_literals_cover_a_bool, R"(
    fun f(b: Bool) -> S32 {
        let n = case b of {
            == true { 1 }
            == false { 0 }
        }
        ret n
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_exhaustive_case_whose_branches_all_return_ends_the_function, R"(
    fun f(x: Opt[S32]) -> S32 {
        case x of {
            is Some[S32](val) { ret val }
            is None() { ret 0 }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_else_after_every_member_is_matched,
  SppCaseBranchUnreachableError, R"(
    fun f(x: Opt[S32]) -> S32 {
        ret case x of {
            is Some[S32](val) { val }
            is None() { 0 }
            else { 1 }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_branch_repeating_an_earlier_one,
  SppCaseBranchUnreachableError, R"(
    fun f(x: S32) -> Void {
        case x of {
            is S32(..) { }
            is S32(..) { }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_else_after_both_bool_literals,
  SppCaseBranchUnreachableError, R"(
    fun f(b: Bool) -> S32 {
        ret case b of {
            == true { 1 }
            == false { 0 }
            else { 2 }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_tuple_of_variants_covered_by_every_corner, R"(
    fun f(x: (Opt[S32], Opt[S32])) -> S32 {
        ret case x of {
            is (Some[S32](val as a), Some[S32](val as b)) { a + b }
            is (Some[S32](val), None()) { val }
            is (None(), Some[S32](val)) { val }
            is (None(), None()) { 0 }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_tuple_of_variants_missing_a_corner,
  SppCaseBranchMissingElseError, R"(
    fun f(x: (Opt[S32], Opt[S32])) -> S32 {
        ret case x of {
            is (Some[S32](val as a), Some[S32](val as b)) { a + b }
            is (Some[S32](val), None()) { val }
            is (None(), None()) { 0 }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_tuple_corner_already_matched,
  SppCaseBranchUnreachableError, R"(
    fun f(x: (Opt[S32], Opt[S32])) -> S32 {
        ret case x of {
            is (Some[S32](val), None()) { val }
            is (None(), _) { 0 }
            is (None(), None()) { 0 }
            else { 1 }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_nested_variant_covered_through_its_field, R"(
    fun f(x: Opt[Opt[S32]]) -> S32 {
        ret case x of {
            is Some[Opt[S32]](val=Some[S32](val)) { val }
            is Some[Opt[S32]](val=None()) { 0 }
            is None() { 0 }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_nested_variant_covered_by_refining_types, R"(
    fun f(x: Opt[Opt[S32]]) -> S32 {
        ret case x of {
            is Some[Some[S32]](val) { val.val }
            is Some[None](..) { 0 }
            is None() { 0 }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_refining_type_leaves_its_member_open,
  SppCaseBranchMissingElseError, R"(
    fun f(x: Opt[Opt[S32]]) -> S32 {
        ret case x of {
            is Some[Some[S32]](val) { val.val }
            is None() { 0 }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CaseExpressionAst,
  test_valid_tuple_of_bools_covered_by_literals, R"(
    fun f(x: (Bool, Bool)) -> S32 {
        ret case x of {
            is (true, true) { 0 }
            is (true, false) { 1 }
            is (false, _) { 2 }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_integer_literals_never_cover,
  SppCaseBranchMissingElseError, R"(
    fun f(x: (S32, Bool)) -> S32 {
        ret case x of {
            is (0, true) { 0 }
            is (0, false) { 1 }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CaseExpressionAst,
  test_invalid_repeated_integer_literal,
  SppCaseBranchUnreachableError, R"(
    fun f(x: (S32, Bool)) -> S32 {
        ret case x of {
            is (0, _) { 0 }
            is (0, true) { 1 }
            else { 2 }
        }
    }
)");
