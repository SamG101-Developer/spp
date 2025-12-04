#include "../test_macros.hpp"
import testex;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClosureExpressionAst,
    test_invalid_mutate_capture,
    SppInvalidMutationError, R"(
    use std::number::U32
    fun f() -> std::void::Void {
        let a = 5_u32
        let x = |b: U32 caps a| { a = b }
        x(123_u32)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClosureExpressionAst,
    test_invalid_accessing_out_of_scope,
    SppIdentifierUnknownError, R"(
    use std::number::U32
    fun f() -> std::void::Void {
        let a = 5_u32
        let x = || a
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClosureExpressionAst,
    test_invalid_different_return_types,
    SppTypeMismatchError, R"(
    use std::number::U32
    fun f() -> std::void::Void {
        let a = 5_u32
        let x = |caps a| case a < 5_u32 { ret true } else { ret 123 }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClosureExpressionAst,
    test_invalid_move_pinned_by_ref,
    SppMoveFromPinLinkedMemoryError, R"(
    use std::number::U32
    use std::function::FunRef

    fun g(func: FunRef[(), U32]) -> std::void::Void { }

    fun f() -> std::void::Void {
        let a = 5_u32
        let x = |caps &a| 123_u32
        g(x)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClosureExpressionAst,
    test_invalid_move_pinned_by_mut,
    SppMoveFromPinLinkedMemoryError, R"(
    use std::number::U32
    use std::function::FunMut

    fun g(func: FunMut[(), U32]) -> std::void::Void { }

    fun f() -> std::void::Void {
        let mut a = 5_u32
        let x = |caps &mut a| 123_u32
        g(x)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClosureExpressionAst,
    test_invalid_lambda_type_mut,
    SppInvalidMutationError, R"(
    use std::number::U32
    use std::function::FunMut

    fun f() -> std::void::Void {
        let a = 5_u32
        let x: FunMut[(), U32] = |caps &mut a| 123_u32
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClosureExpressionAst,
    test_invalid_unknown_capture_variable,
    SppIdentifierUnknownError, R"(
    use std::number::U32

    fun f() -> std::void::Void {
        let a = 5_u32
        let x = |b: U32 caps a, c| { b = a }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClosureExpressionAst,
    test_invalid_with_capture_mov,
    SppUninitializedMemoryUseError, R"(
    use std::number::U32
    fun f() -> std::void::Void {
        let a = "test"
        let x = |caps a| a
        let b = a
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClosureExpressionAst,
    test_valid_simple,
    R"(
    fun f() -> std::void::Void {
        let x = || 5_u32
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClosureExpressionAst,
    test_valid_with_parameters,
    R"(
    use std::number::U32
    fun f() -> std::void::Void {
        let x = |a: U32, b: U32| a + b
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClosureExpressionAst,
    test_valid_with_capture_mov,
    R"(
    use std::number::U32
    fun f() -> std::void::Void {
        let a = 5_u32
        let x = |b: U32, c: U32 caps a| a + b + c
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClosureExpressionAst,
    test_valid_with_capture_ref,
    R"(
    use std::number::U32
    fun f() -> std::void::Void {
        let a = 5_u32
        let x = |mut b: &U32 caps &a| { b = a }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClosureExpressionAst,
    test_valid_with_capture_mut,
    R"(
    use std::number::U32
    fun f() -> std::void::Void {
        let mut a = 5_u32
        let x = |mut b: &mut U32 caps &mut a| { b = a }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClosureExpressionAst,
    test_valid_lambda_type_mov,
    R"(
    use std::number::U32
    use std::function::FunMov

    fun f() -> std::void::Void {
        let a = 5_u32
        let x: FunMov[(), U32] = |caps a| 123_u32
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClosureExpressionAst,
    test_valid_lambda_type_mut,
    R"(
    use std::number::U32
    use std::function::FunMut

    fun f() -> std::void::Void {
        let mut a = 5_u32
        let x: FunMut[(), U32] = |caps &mut a| 123_u32
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClosureExpressionAst,
    test_valid_lambda_type_ref,
    R"(
    use std::number::U32
    use std::function::FunRef

    fun f() -> std::void::Void {
        let a = 5_u32
        let x: FunRef[(), U32] = |caps &a| 123_u32
    }
)");



SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClosureExpressionAst,
    test_valid_correct_return_type,
    R"(
    use std::number::U32
    fun f() -> std::void::Void {
        let a = 5_u32
        let x = |caps a| a
        let mut y = x()
        y = 123_u32
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClosureExpressionAst,
    test_valid_with_capture_mov_use_capture,
    R"(
    use std::number::U32
    fun f() -> std::void::Void {
        let a = "test"
        let x = |caps a| a
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClosureExpressionAst,
    test_invalid_call_fun_mov_twice,
    SppUninitializedMemoryUseError, R"(
    use std::string::Str
    fun f() -> std::void::Void {
        let a = "test"
        let x = |caps a| a
        x()
        x()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClosureExpressionAst,
    test_invalid_call_fun_mut_immutable,
    SppInvalidMutationError, R"(
    use std::string::Str
    fun f() -> std::void::Void {
        let mut a = "test"
        let x = |caps &mut a| { }
        x()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClosureExpressionAst,
    test_invalid_move_borrowed_capture,
    SppMoveFromPinnedMemoryError, R"(
    fun f() -> std::void::Void {
        let some_variable = "hello world"
        let x = |caps &some_variable| 123_u32
        let b = some_variable
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClosureExpressionAst,
    test_invalid_use_moved_capture_as_borrow,
    SppUninitializedMemoryUseError, R"(
    fun g(x: &std::string::Str) -> std::void::Void { }

    fun f() -> std::void::Void {
        let some_variable = "hello world"
        let x = |caps some_variable| 123_u32
        g(&some_variable)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClosureExpressionAst,
    test_valid_containing_ret_statement,
    R"(
    use std::number::U32
    fun f() -> std::void::Void {
        let a = 5_u32
        let x = |caps a| { ret a }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClosureExpressionAst,
    test_invalid_containing_gen_expression,
    SppFunctionSubroutineContainsGenExpressionError, R"(
    use std::number::U32
    fun f() -> std::void::Void {
        let a = 5_u32
        let x = |caps a| { gen a }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClosureExpressionAst,
    test_valid_containing_gen_expression,
    R"(
    use std::number::U32
    fun f() -> std::void::Void {
        let a = 5_u32
        let x = cor |caps a| { gen a }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClosureExpressionAst,
    test_invalid_containing_ret_statement,
    SppCoroutineContainsRetExprExpressionError, R"(
    use std::number::U32
    fun f() -> std::void::Void {
        let a = 5_u32
        let x = cor |caps a| { ret a }
    }
)");
