#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_on_non_callable,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f() -> Void {
        5()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_too_many_args,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f() -> Void {
        f(5)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_arg_name,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: S32) -> Void {
        f(a=1, b=2)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_name_missing,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: S32, b: S32) -> Void {
        f(1)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_name_missing_with_generic_type,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: S32, b: T) -> Void {
        f(1)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_arg_type_mismatch,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: S32) -> Void {
        f("a")
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_missing_explicit_generic,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: S32) -> Void {
        f(1)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_generic_conflict,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: T, b: T) -> Void {
        f(1, "1")
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_unnecessary_explicit_generic_1,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: T) -> Void {
        f[S32](1)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_unnecessary_explicit_generic_2,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: T) -> Void {
        f[Str](1)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_extra_generic,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: T) -> Void {
        f[Bool, Bool](1)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_generic_named,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: T) -> Void {
        f[T=Bool, U=Bool](1)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_generic_explicit_and_inferred,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f[T, U](a: T) -> Void {
        f[Bool](123)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_ambiguous_1,
    SppFunctionCallOverloadAmbiguousError, R"(
    fun f(a: S32) -> Void { }
    fun f[T](a: T) -> Void { }

    fun g() -> Void {
        f(1)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_ambiguous_2,
    SppFunctionCallOverloadAmbiguousError, R"(
    fun f[T](a: T, b: S32) -> Void { }
    fun f[T](a: S32, b: T) -> Void { }

    fun g() -> Void {
        f(1, 2)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_no_params, R"(
    fun f() -> Void {
        f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_single_param, R"(
    fun f(a: S32) -> Void {
        f(1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_multiple_params, R"(
    fun f(a: S32, b: S32) -> Void {
        f(1, 2)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_generic, R"(
    fun f[T](a: T) -> Void {
        f(1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_generic_multiple, R"(
    fun f[T, U](a: T, b: U) -> Void {
        f(1, "1")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_generic_multiple_same_type, R"(
    fun f[T](a: T, b: T) -> Void {
        f(1, 2)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_generic_explicit, R"(
    fun f[T, U]() -> Void {
        f[S32, Str]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_generic_explicit_and_inferred, R"(
    fun f[T, U](a: T) -> Void {
        f[U=Bool](123)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_coroutine_correct_pins, R"(
    cor c(a: &S32) -> Gen[S32] { }

    fun f() -> Void {
        let x = 123
        c(&x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_async_correct_pins, R"(
    fun a(b: &S32) -> Void { }

    fun f() -> Void {
        let x = 123
        async a(&x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_function_call_with_member_access, R"(
    cls TestClass { }
    cls NewClass {
        !public t: TestClass
        !public u: TestClass
    }

    sup TestClass {
        !public fun f(self) -> Void { }
    }

    fun g(n: NewClass) -> Void {
        n.t.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_function_call_with_member_access_2, R"(
    cls TestClass { }
    cls NewClass {
        !public t: TestClass
        !public u: TestClass
    }

    sup TestClass {
        !public fun f(self, t: TestClass) -> Void { }
    }

    fun g(n: NewClass) -> Void {
        n.t.f(n.u)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_function_call_with_member_access_3, R"(
    cls TestClass { }
    cls NewClass {
        !public t: TestClass
        !public u: TestClass
        !public v: TestClass
    }

    sup TestClass {
        !public fun f(self, t: TestClass) -> TestClass { ret t }
    }

    fun g(n: NewClass) -> Void {
        n.t.f(n.u).f(n.v)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_function_call_with_superimposition, R"(
    cls TestClass { }
    sup TestClass {
        !virtual_method
        !public
        fun f(self) -> Void { }
    }

    cls TestClass2 { }
    sup TestClass2 ext TestClass {
        fun f(self) -> Void { }
    }

    fun g() -> Void {
        TestClass2().f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_function_folding_1, R"(
    fun f(a: S32) -> StrView {
        ret "hello world"
    }

    fun g() -> Void {
        let x = (1, 2, 3, 4)
        let mut y = f(x)..
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_function_folding_2, R"(
    fun f(a: S32, b: S32) -> Void { }

    fun g() -> Void {
        let x = (1, 2, 3, 4)
        let y = (1, 2, 3, 4)
        let mut z = f(x, y)..
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_remove_parameter_for_void_substitution, R"(
    cls TestClass[T] { }

    sup [T] TestClass[T] {
        !public fun f(self, a: T) -> Void { }
    }

    fun g() -> Void {
        let x = TestClass[Void]()
        x.f()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_function_folding_2,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: S32, b: S32) -> Void { }
    fun g() -> Void {
        let x = (1, 2, 3, "4")
        let y = (1, 2, 3, "4")
        f(x, y)..
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_function_folding_moving_objects,
    SppUninitializedMemoryUseError, R"(
    fun f(a: S32, b: Str) -> Void { }
    fun g() -> Void {
        let x = (1, 2, 3)
        let y = Str::from("hello world")
        f(x, y)..
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_function_folding_copying_objects, R"(
    fun f(a: S32, b: USize) -> Void { }
    fun g() -> Void {
        let x = (1, 2, 3)
        let y = 0_uz
        f(x, y)..
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_variadic_fixed_type, R"(
    fun g(a: StrView, ..b: Bool) -> Void {
        ret
    }

    fun f() -> Void {
        g("hello", true, false, true, false)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_variadic_single_generic_type, R"(
    fun g[T](a: StrView, ..b: T) -> Void {
        ret
    }

    fun f() -> Void {
        g("hello", 1, 2, 3, 4)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_variadic_mixed_generic_type, R"(
    fun g[..Ts](a: StrView, ..b: Ts) -> Void {
        ret
    }

    fun f() -> Void {
        g("hello", 1, true, "world")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_variadic_given_no_args, R"(
    fun g(a: StrView, ..b: Bool) -> Void {
        ret
    }

    fun f() -> Void {
        g("hello")
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_variadic_mixed_arg_types,
    SppFunctionCallNoValidSignaturesError, R"(
    fun g(a: StrView, ..b: Bool) -> Void {
        ret
    }

    fun f() -> Void {
        g("hello", 1, true, "world")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_keyword_args, R"(
    fun f(a: S32, b: S32) -> Void { }

    fun g() -> Void {
        f(a=1, b=2)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_keyword_args_out_of_order, R"(
    fun f(a: S32, b: S32) -> Void { }

    fun g() -> Void {
        f(b=2, a=1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_mixed_positional_and_keyword, R"(
    fun f(a: S32, b: S32) -> Void { }

    fun g() -> Void {
        f(1, b=2)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_optional_param_omitted, R"(
    fun f(a: S32, b: S32 = 5) -> Void { }

    fun g() -> Void {
        f(1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_optional_param_positional, R"(
    fun f(a: S32, b: S32 = 5) -> Void { }

    fun g() -> Void {
        f(1, 2)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_optional_param_by_name, R"(
    fun f(a: S32, b: S32 = 5) -> Void { }

    fun g() -> Void {
        f(1, b=2)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_ref_self_method, R"(
    cls TestClass { }
    sup TestClass {
        !public fun f(&self) -> Void { }
    }

    fun g(a: TestClass) -> Void {
        a.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_mut_self_method, R"(
    cls TestClass {
        !public x: Bool
    }
    sup TestClass {
        !public fun f(&mut self) -> Void { self.x = true }
    }

    fun g(mut a: TestClass) -> Void {
        a.f()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_mut_self_method_on_immutable,
    SppInvalidMutationError, R"(
    cls TestClass {
        !public x: Bool
    }
    sup TestClass {
        !public fun f(&mut self) -> Void { self.x = true }
    }

    fun g(a: TestClass) -> Void {
        a.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_genonce_auto_resume, R"(
    cor c() -> GenOnce[S32] { }

    fun f() -> Void {
        let x: S32 = c()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_non_cmp_in_cmp_context,
    SppInvalidComptimeOperationError, R"(
    fun g() -> Void { }

    cmp fun f() -> Void {
        g()
    }
)");
