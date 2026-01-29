#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    UnaryExpressionOperatorDerefAst,
    test_invalid_non_borrow_type,
    SppDereferenceInvalidExpressionNonBorrowedTypeError, R"(
    fun f() -> std::void::Void {
        let x = 10
        let y = *x
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    UnaryExpressionOperatorDerefAst,
    test_valid_deref_ref,
    R"(
    fun f(x: &std::boolean::Bool) -> std::void::Void {
        let y = *x
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    UnaryExpressionOperatorDerefAst,
    test_valid_deref_mut,
    R"(
    fun f(mut x: &mut std::boolean::Bool) -> std::void::Void {
        let y = *x
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    UnaryExpressionOperatorDerefAst,
    test_invalid_non_copyable,
    SppInvalidExpressionNonCopyableTypeError, R"(
    fun f(x: &std::string::Str) -> std::string::Str {
        let y = *x
        ret y
    }
)");
