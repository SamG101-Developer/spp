#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    UnaryExpressionOperatorDerefAst,
    test_invalid_non_borrow_type,
    SppDereferenceNonBorrowedTypeError, R"(
    fun f() -> Void {
        let x = 10
        let y = x@
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    UnaryExpressionOperatorDerefAst,
    test_valid_deref_ref,
    R"(
    fun f(x: &Bool) -> Void {
        let y = x@
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    UnaryExpressionOperatorDerefAst,
    test_valid_deref_mut,
    R"(
    fun f(mut x: &mut Bool) -> Void {
        let y = x@
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    UnaryExpressionOperatorDerefAst,
    test_invalid_non_copyable,
    SppNonCopyableTypeError, R"(
    fun f(x: &Str) -> Str {
        let y = x@
        ret y
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    UnaryExpressionOperatorDerefAst,
    test_invalid_non_copyable_mut_borrow,
    SppNonCopyableTypeError, R"(
    fun f(x: &mut Str) -> Str {
        let y = x@
        ret y
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    UnaryExpressionOperatorDerefAst,
    test_valid_move_deref_non_copyable_assignment_target, R"(
    fun f(x: &mut Str) -> Void {
        x@ = Str::from("hello")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    UnaryExpressionOperatorDerefAst,
    test_valid_deref_user_copyable_type, R"(
    cls A { }
    sup A ext Copy { }
    fun f(x: &A) -> Void {
        let mut y = x@
        y = A()
    }
)");
