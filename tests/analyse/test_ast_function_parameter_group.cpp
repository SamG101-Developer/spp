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
        fun f(self) -> Void {
            std::mem::ops::drop(self)
        }
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
        fun f(self, a: S32) -> Void {
            std::mem::ops::drop(self)
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_function_parameter_group_self_opt,
    R"(
    cls A { }
    sup A {
        fun f(self, a: S32 = 0) -> Void {
            std::mem::ops::drop(self)
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_function_parameter_group_self_var,
    R"(
    cls A { }
    sup A {
        fun f(self, ..a: S32) -> Void {
            std::mem::ops::drop(self)
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_function_parameter_group_all_kinds_ordered,
    R"(
    cls A { }
    sup A {
        fun f(self, a: S32, b: S32 = 0, ..c: S32) -> Void {
            std::mem::ops::drop(self)
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_function_parameter_group_req_opt_var,
    R"(
    fun f(a: S32, b: S32 = 0, ..c: S32) -> Void { }
)");

// A variadic parameter holds a pack: in the template it is read element by element, typed as its element; each
// instantiation (one per argument count) reads its tuple, where the index is checked.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_variadic_parameter_element_access, R"(
    fun g(..xs: S32) -> S32 { ret xs.0 + xs.1 }
    fun f() -> Void { let a = g(1, 2) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_variadic_generic_parameter_element_access, R"(
    fun g[..Ts](..a: Ts) -> Void {
        let x = a.0
        std::mem::ops::drop(x)
    }
    fun f() -> Void { g(1, true) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_variadic_comp_parameter_element_access, R"(
    fun g[cmp ..n: Bool]() -> Bool { ret n.0 }
    fun f() -> Void { let b = g[true, false]() }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterGroupAst,
    test_valid_variadic_parameter_called_with_no_arguments, R"(
    fun g(..xs: S32) -> Void { }
    fun f() -> Void { g() }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionParameterGroupAst,
    test_invalid_variadic_parameter_element_out_of_range,
    SppMemberAccessOutOfBoundsError, R"(
    fun g(..xs: S32) -> S32 { ret xs.2 }
    fun f() -> Void { let a = g(1, 2) }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionParameterGroupAst,
    test_invalid_variadic_comp_parameter_element_out_of_range,
    SppMemberAccessOutOfBoundsError, R"(
    fun g[cmp ..n: Bool]() -> Bool { ret n.2 }
    fun f() -> Void { let b = g[true, false]() }
)");
