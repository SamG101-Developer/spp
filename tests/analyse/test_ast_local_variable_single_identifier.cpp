#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableSingleIdentifierAst,
    test_valid_simple, R"(
    fun f() -> Void {
        let x = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableSingleIdentifierAst,
    test_valid_explicit_type, R"(
    fun f() -> Void {
        let x: S32 = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableSingleIdentifierAst,
    test_valid_mutable_reassignment, R"(
    fun f() -> Void {
        let mut x = 123
        x = 456
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableSingleIdentifierAst,
    test_invalid_immutable_reassignment,
    SppInvalidMutationError, R"(
    fun f() -> Void {
        let x = 123
        x = 456
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableSingleIdentifierAst,
    test_valid_mutable_borrow_binding, R"(
    fun f(b: &mut Bool) -> Void {
        let mut x = b
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableSingleIdentifierAst,
    test_valid_move_binding, R"(
    fun f(b: Bool) -> Void {
        let x = b
    }
)");
