#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionParameterOptionalAst,
    test_invalid_function_parameter_optional_invalid_type,
    SppTypeMismatchError, R"(
    fun f(a: std::boolean::Bool = 1) -> std::void::Void { }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterOptionalAst,
    test_valid_function_parameter_optional,
    R"(
    fun f(a: std::boolean::Bool = true) -> std::void::Void { }
)")
