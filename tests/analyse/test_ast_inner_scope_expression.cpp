#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    InnerScopeExpressionAst,
    test_valid_block_returns_final_expression_value, R"(
    fun f() -> S32 {
        ret { 100 }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    InnerScopeExpressionAst,
    test_valid_block_returns_final_of_multiple_members, R"(
    fun f() -> S32 {
        ret {
            let a = 1
            let b = 2
            a + b
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    InnerScopeExpressionAst,
    test_valid_nested_block_returns_value, R"(
    fun f() -> S32 {
        ret { { 100 } }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    InnerScopeExpressionAst,
    test_invalid_block_value_type_mismatch,
    SppTypeMismatchError, R"(
    fun f() -> S32 {
        ret { "hello world" }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    InnerScopeExpressionAst,
    test_invalid_empty_block_returns_void_mismatch,
    SppTypeMismatchError, R"(
    fun f() -> S32 {
        ret { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    InnerScopeExpressionAst,
    test_invalid_unreachable_after_terminating_block,
    SppUnreachableCodeError, R"(
    fun f() -> S32 {
        { ret 100 }
        ret 200
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    InnerScopeExpressionAst,
    test_invalid_use_after_move_out_of_block,
    SppUninitializedMemoryUseError, R"(
    cls Foo { }
    fun f() -> Foo {
        let a = Foo()
        let b = { a }
        let c = a
        ret c
    }
)");
