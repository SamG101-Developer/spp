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
        let a = 5_u32
        let x = (caps &a) 123_u32
        g(x)
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
        let a = "test"
        let x = (caps a) a
        let b = a
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
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClosureExpressionAst,
    test_invalid_closure_call_fun_mov_twice,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let a = "test"
        let x = (caps a) a
        x()
        x()
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
        let some_variable = "hello world"
        let x = (caps some_variable) 123_u32
        g(&some_variable)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClosureExpressionAst,
    test_valid_closure_containing_ret_statement,
    R"(
    fun f() -> Void {
        let a = 5_u32
        let x = (caps a) { ret a }
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
