#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionParameterGroupAst,
    test_invalid_function_parameter_group_duplicate_parameter_name,
    SppIdentifierDuplicateError, R"(
    fun f(a: S32, b: S32, a: S32) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionParameterGroupAst,
    test_invalid_function_parameter_group_order_invalid_req_self,
    SppOrderInvalidError, R"(
    cls A { }
    sup A {
        fun f(a: S32, self) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionParameterGroupAst,
    test_invalid_function_parameter_group_order_invalid_opt_self,
    SppOrderInvalidError, R"(
    cls A { }
    sup A {
        fun f(a: S32 = 0, self) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionParameterGroupAst,
    test_invalid_function_parameter_group_order_invalid_var_self,
    SppOrderInvalidError, R"(
    cls A { }
    sup A {
        fun f(..a: S32, self) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionParameterGroupAst,
    test_invalid_function_parameter_group_order_invalid_opt_req,
    SppOrderInvalidError, R"(
    fun f(a: S32 = 0, b: S32) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionParameterGroupAst,
    test_invalid_function_parameter_group_order_invalid_var_req,
    SppOrderInvalidError, R"(
    fun f(..a: S32, b: S32) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionParameterGroupAst,
    test_invalid_function_parameter_group_order_invalid_var_opt,
    SppOrderInvalidError, R"(
    fun f(..a: S32, b: S32 = 0) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionParameterGroupAst,
    test_invalid_function_parameter_group_multiple_self,
    SppMultipleSelfParametersError, R"(
    cls A { }
    sup A {
        fun f(self, &mut self) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionParameterGroupAst,
    test_invalid_function_parameter_group_multiple_variadic,
    SppMultipleVariadicParametersError, R"(
    fun f(..a: S32, ..b: S32) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_function_parameter_group_empty,
    R"(
    fun f() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_function_parameter_group_self,
    R"(
    cls A { }
    sup A {
        fun f(self) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_function_parameter_group_req,
    R"(
    fun f(a: S32) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_function_parameter_group_opt,
    R"(
    fun f(a: S32 = 0) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_function_parameter_group_var,
    R"(
    fun f(..a: S32) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_function_parameter_group_req_opt,
    R"(
    fun f(a: S32, b: S32 = 0) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_function_parameter_group_req_var,
    R"(
    fun f(a: S32, ..b: S32) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_function_parameter_group_opt_var,
    R"(
    fun f(a: S32 = 0, ..b: S32) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_function_parameter_group_self_req,
    R"(
    cls A { }
    sup A {
        fun f(self, a: S32) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_function_parameter_group_self_opt,
    R"(
    cls A { }
    sup A {
        fun f(self, a: S32 = 0) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_function_parameter_group_self_var,
    R"(
    cls A { }
    sup A {
        fun f(self, ..a: S32) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_function_parameter_group_all_kinds_ordered,
    R"(
    cls A { }
    sup A {
        fun f(self, a: S32, b: S32 = 0, ..c: S32) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_function_parameter_group_req_opt_var,
    R"(
    fun f(a: S32, b: S32 = 0, ..c: S32) -> Void { }
)");
