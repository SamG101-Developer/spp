#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementUninitializedAst,
    test_valid_uninitialized, R"(
    fun f() -> Void {
        let x: Bool
        x = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementUninitializedAst,
    test_valid_initialize_then_use, R"(
    fun f() -> Void {
        let x: Bool
        x = false
        let y = x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LetStatementUninitializedAst,
    test_invalid_use_before_initialization,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let x: Bool
        let y = x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementUninitializedAst,
    test_valid_conv_mov, R"(
    fun f(b: Bool) -> Void {
        let mut x: Bool
        x = b
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementUninitializedAst,
    test_valid_conv_mut, R"(
    fun f(b: &mut Bool) -> Void {
        let mut x: &mut Bool
        x = b
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementUninitializedAst,
    test_valid_conv_ref, R"(
    fun f(b: &Bool) -> Void {
        let mut x: &Bool
        x = b
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LetStatementUninitializedAst,
    test_valid_variant_type, R"(
    fun f() -> Void {
        let x: StrView or Bool
        x = false
    }
)");
