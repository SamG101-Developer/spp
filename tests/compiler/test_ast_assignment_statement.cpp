#include "test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentStatementAst,
  test_valid_variable_target, R"(
    fun f() -> Void {
        let mut a = 1
        a = 2
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentStatementAst,
  test_valid_owned_attribute_target, R"(
    cls A {
        !public
        b: Bool
    }

    fun f(mut a: A) -> A {
        a.b = true
        ret a
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentStatementAst,
  test_valid_mutably_borrowed_attribute_target, R"(
    cls A {
        !public
        b: Bool
    }

    fun f(a: &mut A) -> Void {
        a.b = true
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentStatementAst,
  test_valid_mutable_val_immutable_borrow, R"(
    fun f(mut a: &Bool, b: &Bool) -> Void {
        a = b
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentStatementAst,
  test_valid_non_initialized_immutable_variable_target, R"(
    fun f() -> Void {
        let a: Bool
        a = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentStatementAst,
  test_valid_non_initialized_mutable_variable_targe, R"(
    fun f() -> Void {
        let mut a: Bool
        a = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentStatementAst,
  test_valid_non_initialized_mutable_variable_target_then_reassign, R"(
    fun f() -> Void {
        let mut a: Bool
        a = false
        a = true
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentStatementAst,
  test_valid_convention_mismatch_but_valid_coercion, R"(
    fun f(b: &mut Bool) -> Void {
        let mut x: &Bool
        x = b
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentStatementAst,
  test_invalid_convention_mismatch_1,
  SppTypeMismatchError, R"(
    fun f(b: Bool) -> Void {
        let mut x: &mut Bool
        x = b
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentStatementAst,
  test_invalid_convention_mismatch_2,
  SppTypeMismatchError, R"(
    fun f(b: Bool) -> Void {
        let mut x: &Bool
        x = b
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentStatementAst,
  test_invalid_convention_mismatch_3,
  SppTypeMismatchError, R"(
    fun f(b: &mut Bool) -> Void {
        let mut x: Bool
        x = b
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentStatementAst,
  test_invalid_convention_mismatch_4,
  SppTypeMismatchError, R"(
    fun f(b: &Bool) -> Void {
        let mut x: &mut Bool
        x = b
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentStatementAst,
  test_invalid_convention_mismatch_5,
  SppTypeMismatchError, R"(
    fun f(b: &Bool) -> Void {
        let mut x: Bool
        x = b
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentStatementAst,
  test_invalid_type_mismatch_variable_target,
  SppTypeMismatchError, R"(
    fun f() -> Void {
        let mut a = 1
        a = "2"
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentStatementAst,
  test_invalid_type_mismatch_attribute_target,
  SppTypeMismatchError, R"(
    cls A {
        !public
        b: Bool
    }

    fun f(mut a: A) -> Void {
        a.b = "1"
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentStatementAst,
  test_invalid_immutable_variable_target,
  SppInvalidMutationError, R"(
    fun f() -> Void {
        let a = 1
        a = 2
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentStatementAst,
  test_invalid_immutable_attribute_target,
  SppInvalidMutationError, R"(
    cls A {
        !public
        b: Bool
    }

    fun f(a: A) -> Void {
        a.b = true
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentStatementAst,
  test_invalid_immutable_borrow_attribute_target,
  SppInvalidMutationError, R"(
    cls A {
        !public
        b: Bool
    }

    fun f(a: &A) -> Void {
        a.b = true
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentStatementAst,
  test_invalid_mutable_val_immutable_borrow_attribute_target,
  SppInvalidMutationError, R"(
    cls A {
        !public
        b: Bool
    }

    fun f(mut a: &A) -> Void {
        a.b = true
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentStatementAst,
  test_invalid_non_initialized_immutable_variable_then_reassign,
  SppInvalidMutationError, R"(
    fun f() -> Void {
        let a: Bool
        a = false
        a = true
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentStatementAst,
  test_valid_assign_multi, R"(
    fun f() -> Void {
        let mut a = 1
        let mut b = 2
        a, b = 3, 4
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentStatementAst,
  test_valid_assign_multi_swap, R"(
    fun f() -> Void {
        let mut a = 1
        let mut b = 2
        a, b = b, a
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentStatementAst,
  test_valid_assign_multi_rotate, R"(
    fun f() -> Void {
        let mut a = 1
        let mut b = 2
        let mut c = 3
        a, b, c = b, c, a
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentStatementAst,
  test_valid_assign_with_mutable_deref, R"(
    fun f(x: &mut S32) -> Void {
        x@ = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentStatementAst,
  test_valid_assign_with_mutable_index_deref, R"(
    fun f(x: &mut Vec[S32]) -> Void {
        x[mut 0_uz]@ = 123
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentStatementAst,
  test_invalid_assign_with_immutable_deref,
  SppInvalidMutationError, R"(
    fun f(x: &S32) -> Void {
        x@ = 123
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentStatementAst,
  test_invalid_assign_with_immutable_index_deref,
  SppInvalidMutationError, R"(
    fun f(x: &mut Vec[S32]) -> Void {
        x[0_uz]@ = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentStatementAst,
  test_valid_assign_into_mutable_slice, R"(
    fun f(x: &mut Vec[S32], v: &View[S32]) -> Void {
        x[mut 0_uz to 2_uz]@ = v@
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentStatementAst,
  test_invalid_assign_into_immutable_slice,
  SppInvalidMutationError, R"(
    fun f(x: &mut Vec[S32], v: &View[S32]) -> Void {
        x[0_uz to 2_uz]@ = v@
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentStatementAst,
  test_invalid_assign_into_mutable_slice_type_mismatch,
  SppTypeMismatchError, R"(
    fun f(x: &mut Vec[S32]) -> Void {
        x[mut 0_uz to 2_uz]@ = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC_NO_MAIN(
    AssignmentStatementAst,
    test_valid_assign_back_a_nested_moved_out_part, R"(
    cls Inner { !public val: Str }
    cls Outer { !public inner: Inner }

    fun main() -> Void {
        let mut o = Outer(inner=Inner(val=Str::new()))
        let v = o.inner.val
        drop(v)
        o.inner.val = Str::new()
        let i = o.inner
        drop(i)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC_NO_MAIN(
    AssignmentStatementAst,
    test_invalid_assign_into_a_moved_out_container,
    SppUninitializedMemoryUseError, R"(
    cls Inner { !public val: Str }
    cls Outer { !public inner: Inner }

    fun main() -> Void {
        let mut o = Outer(inner=Inner(val=Str::new()))
        let i = o.inner
        drop(i)
        o.inner.val = Str::new()
    }
)");

// An immutable may be given its value once on each path: the branches of a "case" are alternatives, not a sequence.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  AssignmentStatementAst,
  test_valid_non_initialized_immutable_given_its_value_once_per_branch, R"(
    fun f(c: Bool) -> S32 {
        let x: S32
        case c { x = 1 } else { x = 2 }
        ret x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentStatementAst,
  test_invalid_non_initialized_immutable_given_a_value_on_some_path_then_assigned,
  SppInvalidMutationError, R"(
    fun f(c: Bool) -> Void {
        let x: S32
        case c { x = 1 }
        x = 2
    }
)");

// Moving the value out does not make an immutable take another one.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentStatementAst,
  test_invalid_non_initialized_immutable_given_then_moved_then_assigned,
  SppInvalidMutationError, R"(
    fun take(s: Str) -> Void { }

    fun f() -> Void {
        let x: Str
        x = Str::from("a")
        take(x)
        x = Str::from("b")
        take(x)
    }
)");

// Given and moved on one path, never given on the other: on the join neither path holds it, but one gave it a value.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AssignmentStatementAst,
  test_invalid_non_initialized_immutable_given_then_moved_on_one_path_then_assigned,
  SppInvalidMutationError, R"(
    fun take(s: Str) -> Void { }

    fun f(c: Bool) -> Void {
        let x: Str
        case c { } else {
            x = Str::from("a")
            take(x)
        }
        x = Str::from("b")
        take(x)
    }
)");
