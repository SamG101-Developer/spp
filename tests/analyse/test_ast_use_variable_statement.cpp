#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestUseVariableStatementAst,
    test_valid_use_variable_statement_reduction,
    R"(
    use std::abort::abort
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestUseVariableStatementAst,
    test_valid_use_variable_statement_functional,
    R"(
    use std::abort::abort

    fun f() -> ! {
        abort()
    }
)");
