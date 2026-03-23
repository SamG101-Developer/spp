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


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUseVariableStatementAst,
    test_invalid_use_variable_statement_undefined_constant,
    SppIdentifierUnknownError, R"(
    use std::abort::invalid
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUseVariableStatementAst,
    test_invalid_use_variable_statement_namespace_identifier,
    SppIdentifierUnknownError, R"(
    use std::abort
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUseVariableStatementAst,
    test_invalid_use_variable_statement_undefined_namespace,
    SppIdentifierUnknownError, R"(
    use std::other::abort
)");
