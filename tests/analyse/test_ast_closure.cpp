#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionAst,
  test_invalid_closure_mutate_capture,
  SppInvalidMutationError, R"(
    fun f() -> Void {
        let a = 5_u32
        let x = (b: U32 caps a) { a = b }
        x(123_u32)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionAst,
  test_invalid_closure_accessing_out_of_scope,
  SppIdentifierUnknownError, R"(
    fun f() -> Void {
        let a = 5_u32
        let x = () a
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionAst,
  test_invalid_closure_different_return_types,
  SppTypeMismatchError, R"(
    fun f() -> Void {
        let a = 5_u32
        let x = (caps a) case a < 5_u32 { ret true } else { ret 123 }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionAst,
  test_invalid_closure_move_pinned_by_ref,
  SppMovingEscapingBorrowedMemoryError, R"(
    fun g(func: FunRef[(), U32]) -> Void { }

    fun f() -> Void {
        let a = Str::from("x")
        let x = (caps &a) 123_u32
        g(x)
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionAst,
  test_invalid_closure_move_pinned_by_mut,
  SppMovingEscapingBorrowedMemoryError, R"(
    fun g(func: FunMut[(), U32]) -> Void { }

    fun f() -> Void {
        let mut a = 5_u32
        let x = (caps &mut a) 123_u32
        g(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionAst,
  test_invalid_closure_type_mut,
  SppInvalidMutationError, R"(
    fun f() -> Void {
        let a = 5_u32
        let x: FunMut[(), U32] = (caps &mut a) 123_u32
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionAst,
  test_invalid_closure_unknown_capture_variable,
  SppIdentifierUnknownError, R"(
    fun f() -> Void {
        let a = 5_u32
        let x = (b: U32 caps a, c) { b = a }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionAst,
  test_invalid_closure_with_capture_mov,
  SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let a = Str::from("test")
        let x = (caps a) a
        let b = a
        std::mem::ops::drop(x)
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_simple,
  R"(
    fun f() -> Void {
        let x = () 5_u32
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_with_parameters,
  R"(
    fun f() -> Void {
        let x = (a: U32, b: U32) a + b
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_with_capture_mov,
  R"(
    fun f() -> Void {
        let a = 5_u32
        let x = (b: U32, c: U32 caps a) a + b + c
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_with_capture_ref,
  R"(
    fun f() -> Void {
        let a = 5_u32
        let x = (mut b: &U32 caps &a) { b = a }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_with_capture_mut,
  R"(
    fun f() -> Void {
        let mut a = 5_u32
        let x = (mut b: &mut U32 caps &mut a) { b = a }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_type_mov,
  R"(
    fun f() -> Void {
        let a = 5_u32
        let x: FunMov[(), U32] = (caps a) 123_u32
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_type_mut,
  R"(
    fun f() -> Void {
        let mut a = 5_u32
        let x: FunMut[(), U32] = (caps &mut a) 123_u32
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_type_ref,
  R"(
    fun f() -> Void {
        let a = 5_u32
        let x: FunRef[(), U32] = (caps &a) 123_u32
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_no_capture_infers_fun_ref,
  R"(
    fun f() -> Void {
        let x: FunRef[(), U32] = () 5_u32
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_multiple_mov_captures,
  R"(
    fun f() -> Void {
        let a = 5_u32
        let b = 6_u32
        let x: FunMov[(), U32] = (caps a, b) 123_u32
        let result = x()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_mixed_mov_and_ref_captures_infers_fun_mov,
  R"(
    fun f() -> Void {
        let a = 5_u32
        let b = 6_u32
        let x: FunMov[(), U32] = (caps a, &b) 123_u32
        let result = x()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_mixed_ref_and_mut_captures_infers_fun_mut,
  R"(
    fun f() -> Void {
        let a = 5_u32
        let mut b = 6_u32
        let x: FunMut[(), U32] = (caps &a, &mut b) 123_u32
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_correct_return_type,
  R"(
    fun f() -> Void {
        let a = 5_u32
        let x = (caps a) a
        let mut y = x()
        y = 123_u32
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_with_capture_mov_use_capture,
  R"(
    fun f() -> Void {
        let a = "test"
        let x = (caps a) a
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionAst,
  test_invalid_closure_call_fun_mov_twice,
  SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let a = "test"
        let x = (caps a) a

        let z = x()
        let z = x()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionAst,
  test_invalid_closure_call_fun_mut_immutable,
  SppInvalidMutationError, R"(
    fun f() -> Void {
        let mut a = "test"
        let x = (caps &mut a) { }
        x()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionAst,
  test_invalid_closure_move_borrowed_capture,
  SppMovingEscapingBorrowedMemoryError, R"(
    fun f() -> Void {
        let some_variable = Str::from("hello world")
        let x = (caps &some_variable) 123_u32
        let b = some_variable
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionAst,
  test_invalid_closure_use_moved_capture_as_borrow,
  SppUninitializedMemoryUseError, R"(
    fun g(x: &StrView) -> Void { }

    fun f() -> Void {
        let some_variable = Str::from("hello world")
        let x = (caps some_variable) 123_u32
        g(&some_variable)
        std::mem::ops::drop(x)
        std::mem::ops::drop(some_variable)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_containing_ret_statement,
  R"(
    fun f() -> Void {
        let a = 5_u32
        let x = (caps a) { ret a }
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionAst,
  test_invalid_closure_containing_gen_expression,
  SppFunctionSubroutineContainsGenExpressionError, R"(
    fun f() -> Void {
        let a = 5_u32
        let x = (caps a) { gen a }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_containing_gen_expression,
  R"(
    fun f() -> Void {
        let a = 5_u32
        let x = cor (caps a) { gen a }
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionAst,
  test_invalid_closure_containing_ret_statement,
  SppCoroutineContainsReturnStatementError, R"(
    fun f() -> Void {
        let a = 5_u32
        let x = cor (caps a) { ret a }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_parameters_without_defaults,
  R"(
    fun f() -> Void {
        let x = (a: U32, b: U32) a
        std::mem::ops::drop(x(1_u32, 2_u32))
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionAst,
  test_invalid_move_of_a_value_borrowed_by_a_closure_held_elsewhere,
  SppMovingEscapingBorrowedMemoryError, R"(
    fun hold[F: FunRef[(), U32]](f: F) -> F { ret f }

    fun f() -> Void {
        let a = Str::from("x")
        let handle = hold((caps &a) 123_u32)
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_returning_a_closure_with_a_declared_function_return_type, R"(
    fun f() -> Void {
        let chooser = () -> std::function::FunMov[(), S32] { ret () -> S32 { ret 5 } }
        std::mem::ops::drop(chooser)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_returning_a_closure_with_an_inferred_return_type, R"(
    fun f() -> Void {
        let chooser = () { ret () -> S32 { ret 5 } }
        std::mem::ops::drop(chooser)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_nested_closure_captures_from_the_outer_closure, R"(
    fun f() -> Void {
        let n = 5
        let outer = (caps n) -> S32 {
            let inner = (caps n) -> S32 { ret n }
            ret inner()
        }
        let r = outer()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_triply_nested_closures, R"(
    fun f() -> Void {
        let n = 3
        let a = (caps n) -> S32 {
            let b = (caps n) -> S32 {
                let c = (caps n) -> S32 { ret n }
                ret c()
            }
            ret b()
        }
        let r = a()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_nested_closure_inside_a_method_resolves_self, R"(
    cls A { !public n: S32 }
    sup A {
        !public
        fun m(&self) -> A {
            let outer = () -> Self {
                let inner = () -> Self { ret A(n=2) }
                ret inner()
            }
            ret outer()
        }
    }
    fun f() -> Void {
        let a = A(n=1)
        let b = a.m()
        std::mem::ops::drop(a)
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_nested_closure_names_the_outer_closures_parameter, R"(
    fun f() -> Void {
        let outer = (x: S32) -> S32 {
            let inner = (caps x) -> S32 { ret x }
            ret inner()
        }
        let r = outer(7)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_inside_a_method_resolves_self, R"(
    cls A { !public n: S32 }
    sup A {
        !public
        fun m(&self) -> A {
            let f = () -> Self { ret A(n=2) }
            ret f()
        }
    }
    fun f() -> Void {
        let a = A(n=1)
        let b = a.m()
        std::mem::ops::drop(a)
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionAst,
  test_invalid_closure_owning_a_moved_capture_not_consumed,
  SppLinearValueNotConsumedError, R"(
    fun f() -> Void {
        let s = Str::from("a")
        let x = (caps s) 1_u32
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionAst,
  test_invalid_closure_parameter_not_consumed_expression_body,
  SppLinearValueNotConsumedError, R"(
    fun f() -> Void {
        let x = (a: Str) 1_u32
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionAst,
  test_invalid_closure_parameter_not_consumed_braced_body,
  SppLinearValueNotConsumedError, R"(
    fun f() -> Void {
        let x = (a: Str) { 1_u32 }
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionAst,
  test_invalid_closure_parameter_not_consumed_ret_body,
  SppLinearValueNotConsumedError, R"(
    fun f() -> Void {
        let x = (a: Str) { ret 1_u32 }
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_parameter_consumed_before_ret, R"(
    fun f() -> Void {
        let x = (a: Str) {
            std::mem::ops::drop(a)
            ret 1_u32
        }
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionAst,
  test_valid_closure_moved_capture_not_consumed_by_body, R"(
    fun f() -> Void {
        let s = Str::from("a")
        let x = (caps s) 1_u32
        std::mem::ops::drop(x)
    }
)");
