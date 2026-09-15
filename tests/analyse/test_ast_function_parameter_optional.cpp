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
    fun f(a: Str = Str::from("hello")) -> Void {
        std::mem::ops::drop(a)
    }
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

SPP_TEST_SHOULD_PASS_SEMANTIC(
  FunctionParameterOptionalAst,
  test_valid_function_parameter_optional_call_default_omitted_by_an_earlier_caller, R"(
    cls A { }
    fun g() -> Void {
        h()
    }
    fun make() -> A { ret A() }
    fun h(x: A = make()) -> Void {
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  FunctionParameterOptionalAst,
  test_valid_function_parameter_optional_call_default_omitted_by_a_later_caller, R"(
    cls A { }
    fun make() -> A { ret A() }
    fun h(x: A = make()) -> Void {
        std::mem::ops::drop(x)
    }
    fun g() -> Void {
        h()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  FunctionParameterOptionalAst,
  test_valid_method_parameter_optional_call_default_omitted, R"(
    cls A { }
    sup A {
        !public fun make() -> A { ret A() }
        !public fun m(&self, other: A = A::make()) -> Void { std::mem::ops::drop(other) }
    }
    fun g() -> Void {
        let a = A()
        a.m()
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  FunctionParameterOptionalAst,
  test_valid_generic_function_parameter_optional_call_default_omitted, R"(
    cls A { }
    fun make() -> A { ret A() }
    fun h[T](x: A = make()) -> Void {
        std::mem::ops::drop(x)
    }
    fun g() -> Void {
        h[S32]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  FunctionParameterOptionalAst,
  test_valid_generic_method_parameter_optional_default_names_a_private_helper, R"(
    cls A { }
    sup A {
        fun helper() -> A { ret A() }
        !public fun m[T](&self, x: A = A::helper()) -> Void { std::mem::ops::drop(x) }
    }
    fun g() -> Void {
        let a = A()
        a.m[S32]()
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FunctionParameterOptionalAst,
  test_invalid_function_parameter_optional_case_default,
  SppInvalidDefaultValueError, R"(
    fun f(a: S32 = case true { 1_s32 } else { 2_s32 }) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FunctionParameterOptionalAst,
  test_invalid_function_parameter_optional_loop_default,
  SppInvalidDefaultValueError, R"(
    fun f(a: S32 = loop true { }) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FunctionParameterOptionalAst,
  test_invalid_function_parameter_optional_ret_in_a_scope_default,
  SppInvalidDefaultValueError, R"(
    fun f(a: S32 = { ret 1_s32 }) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FunctionParameterOptionalAst,
  test_invalid_function_parameter_optional_early_return_default,
  SppInvalidDefaultValueError, R"(
    fun g() -> Opt[S32] { ret Some(val=1_s32) }
    fun f(a: S32 = g()?) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FunctionParameterOptionalAst,
  test_invalid_function_parameter_optional_case_nested_in_an_argument_default,
  SppInvalidDefaultValueError, R"(
    fun g(x: S32) -> S32 { ret x }
    fun f(a: S32 = g(case true { 1_s32 } else { 2_s32 })) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FunctionParameterOptionalAst,
  test_invalid_function_parameter_optional_scope_default,
  SppInvalidDefaultValueError, R"(
    fun f(a: S32 = {
        let x = 1_s32
        x
    }) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FunctionParameterOptionalAst,
  test_invalid_function_parameter_optional_closure_default,
  SppInvalidDefaultValueError, R"(
    fun f(a: FunRef[(S32,), S32] = (x: S32) { ret x }) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FunctionParameterOptionalAst,
  test_invalid_function_parameter_optional_block_nested_in_an_argument_default,
  SppInvalidDefaultValueError, R"(
    fun g(x: S32) -> S32 { ret x }
    fun f(a: S32 = g({ 1_s32 })) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  FunctionParameterOptionalAst,
  test_valid_function_parameter_optional_tuple_default, R"(
    fun f(a: (S32, Bool) = (1_s32, true)) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionParameterOptionalAst,
    test_invalid_function_parameter_optional_async_default,
    SppInvalidDefaultValueError, R"(
    fun g() -> S32 { ret 1_s32 }
    fun f(a: Fut[S32] = async g()) -> Void { std::mem::ops::drop(a) }
)");
