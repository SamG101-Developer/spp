#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionParameterOptionalAst,
    test_invalid_function_parameter_optional_invalid_type,
    SppTypeMismatchError, R"(
    fun f(a: Bool = 1) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionParameterOptionalAst,
    test_invalid_function_parameter_optional_invalid_ast,
    SppInvalidPrimaryExpressionError, R"(
    fun f(a: Bool = Bool) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterOptionalAst,
    test_valid_function_parameter_optional, R"(
    fun f(a: Bool = true) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterOptionalAst,
    test_valid_function_parameter_optional_non_copyable_default, R"(
    fun f(a: Str = Str::from("hello")) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterOptionalAst,
    test_valid_function_parameter_optional_expression_default, R"(
    fun f(a: S32 = 1 + 2) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterOptionalAst,
    test_valid_function_parameter_optional_variant_default, R"(
    fun f(a: Opt[S32] = None) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionParameterOptionalAst,
    test_invalid_function_parameter_optional_variant_default_not_member,
    SppTypeMismatchError, R"(
    fun f(a: Opt[S32] = true) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterOptionalAst,
    test_valid_function_parameter_optional_omitted_at_call_site, R"(
    fun f(a: S32 = 5) -> S32 { ret a }

    fun g() -> Void {
        let x = f()
    }
)");
