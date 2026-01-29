#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    InnerScopeAst,
    test_invalid_unreachable_code_in_function,
    SppUnreachableCodeError, R"(
    fun f() -> std::number::S32 {
        ret 100
        ret 200
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    InnerScopeAst,
    test_invalid_unreachable_code_in_inner_scope,
    SppUnreachableCodeError, R"(
    fun f() -> std::number::S32 {
        {
            ret 100
            f()
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    InnerScopeAst,
    test_valid_inner_scope, R"(
    fun f() -> std::number::S32 {
        {
            ret 100
        }
        ret 200
    }
)");
