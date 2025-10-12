#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_on_non_callable,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f() -> std::void::Void {
        5()
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_abstract_function,
    SppFunctionCallNoValidSignaturesError, R"(
    cls TestClass { }
    sup TestClass {
        @abstract_method
        fun f(&self) -> std::void::Void { }
    }

    fun g() -> std::void::Void {
        TestClass().f()
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_too_many_args,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f() -> std::void::Void {
        f(5)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_arg_name,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: std::number::S32) -> std::void::Void {
        f(a=1, b=2)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_name_missing,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: std::number::S32, b: std::number::S32) -> std::void::Void {
        f(1)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_name_missing_with_generic_type,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: std::number::S32, b: T) -> std::void::Void {
        f(1)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_arg_type_mismatch,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: std::number::S32) -> std::void::Void {
        f("a")
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_missing_explicit_generic,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: std::number::S32) -> std::void::Void {
        f(1)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_generic_conflict,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: T, b: T) -> std::void::Void {
        f(1, "1")
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_unnecessary_explicit_generic_1,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: T) -> std::void::Void {
        f[std::number::S32](1)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_unnecessary_explicit_generic_2,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: T) -> std::void::Void {
        f[std::string::Str](1)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_extra_generic,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: T) -> std::void::Void {
        f[std::boolean::Bool, std::boolean::Bool](1)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_generic_named,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: T) -> std::void::Void {
        f[T=std::boolean::Bool, U=std::boolean::Bool](1)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_generic_explicit_and_inferred,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f[T, U](a: T) -> std::void::Void {
        f[std::boolean::Bool](123)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_ambiguous_1,
    SppFunctionCallOverloadAmbiguousError, R"(
    fun f(a: std::number::S32) -> std::void::Void { }
    fun f[T](a: T) -> std::void::Void { }

    fun g() -> std::void::Void {
        f(1)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_func_call_ambiguous_2,
    SppFunctionCallOverloadAmbiguousError, R"(
    fun f[T](a: T, b: std::number::S32) -> std::void::Void { }
    fun f[T](a: std::number::S32, b: T) -> std::void::Void { }

    fun g() -> std::void::Void {
        f(1, 2)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_no_params, R"(
    fun f() -> std::void::Void {
        f()
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_single_param, R"(
    fun f(a: std::number::S32) -> std::void::Void {
        f(1)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_multiple_params, R"(
    fun f(a: std::number::S32, b: std::number::S32) -> std::void::Void {
        f(1, 2)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_generic, R"(
    fun f[T](a: T) -> std::void::Void {
        f(1)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_generic_multiple, R"(
    fun f[T, U](a: T, b: U) -> std::void::Void {
        f(1, "1")
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_generic_multiple_same_type, R"(
    fun f[T](a: T, b: T) -> std::void::Void {
        f(1, 2)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_generic_explicit, R"(
    fun f[T, U]() -> std::void::Void {
        f[std::number::S32, std::string::Str]()
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_generic_explicit_and_inferred, R"(
    fun f[T, U](a: T) -> std::void::Void {
        f[U=std::boolean::Bool](123)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_coroutine_correct_pins, R"(
    cor c(a: &std::number::S32) -> std::generator::Gen[std::number::S32] { }

    fun f() -> std::void::Void {
        let x = 123
        c(&x)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_func_call_async_correct_pins, R"(
    fun a(b: &std::number::S32) -> std::void::Void { }

    fun f() -> std::void::Void {
        let x = 123
        async a(&x)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_function_call_with_member_access, R"(
    cls TestClass { }
    cls NewClass {
        t: TestClass
        u: TestClass
    }

    sup TestClass {
        fun f(self) -> std::void::Void { }
    }

    fun g(n: NewClass) -> std::void::Void {
        n.t.f()
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_function_call_with_member_access_2, R"(
    cls TestClass { }
    cls NewClass {
        t: TestClass
        u: TestClass
    }

    sup TestClass {
        fun f(self, t: TestClass) -> std::void::Void { }
    }

    fun g(n: NewClass) -> std::void::Void {
        n.t.f(n.u)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_function_call_with_member_access_3, R"(
    cls TestClass { }
    cls NewClass {
        t: TestClass
        u: TestClass
        v: TestClass
    }

    sup TestClass {
        @no_impl
        fun f(self, t: TestClass) -> TestClass { }
    }

    fun g(n: NewClass) -> std::void::Void {
        n.t.f(n.u).f(n.v)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_function_call_with_superimposition, R"(
    cls TestClass { }
    sup TestClass {
        @virtual_method
        fun f(self) -> std::void::Void { }
    }

    cls TestClass2 { }
    sup TestClass2 ext TestClass {
        fun f(self) -> std::void::Void { }
    }

    fun g() -> std::void::Void {
        TestClass2().f()
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_function_folding_1, R"(
    fun f(a: std::number::S32) -> std::string::Str {
        ret "hello world"
    }

    fun g() -> std::void::Void {
        let x = (1, 2, 3, 4)
        let mut y = f(x)..
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_postfix_function_folding_2, R"(
    fun f(a: std::number::S32, b: std::number::S32) -> std::void::Void { }

    fun g() -> std::void::Void {
        let x = (1, 2, 3, 4)
        let y = (1, 2, 3, 4)
        let mut z = f(x, y)..
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_remove_parameter_for_void_substitution, R"(
    cls TestClass[T] { }

    sup [T] TestClass[T] {
        fun f(self, a: T) -> std::void::Void { }
    }

    fun g() -> std::void::Void {
        let x = TestClass[std::void::Void]()
        x.f()
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_function_folding_1,
    SppFunctionFoldTupleLengthMismatchError, R"(
    fun f(a: std::number::S32, b: std::number::S32) -> std::void::Void { }
    fun g() -> std::void::Void {
        let x = (1, 2, 3, 4)
        let y = (1, 2)
        f(x, y)..
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_function_folding_2,
    SppFunctionFoldTupleElementTypeMismatchError, R"(
    fun f(a: std::number::S32, b: std::number::S32) -> std::void::Void { }
    fun g() -> std::void::Void {
        let x = (1, 2, 3, "4")
        let y = (1, 2, 3, "4")
        f(x, y)..
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_function_folding_moving_objects,
    SppUninitializedMemoryUseError, R"(
    fun f(a: std::number::S32, b: std::string::Str) -> std::void::Void { }
    fun g() -> std::void::Void {
        let x = (1, 2, 3)
        let y = "hello world"
        f(x, y)..
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_postfix_function_folding_copying_objects, R"(
    fun f(a: std::number::S32, b: std::number::USize) -> std::void::Void { }
    fun g() -> std::void::Void {
        let x = (1, 2, 3)
        let y = 0_uz
        f(x, y)..
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_variadic_fixed_type, R"(
    fun g(a: std::string::Str, ..b: std::boolean::Bool) -> std::void::Void {
        ret
    }

    fun f() -> std::void::Void {
        g("hello", true, false, true, false)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_variadic_single_generic_type, R"(
    fun g[T](a: std::string::Str, ..b: T) -> std::void::Void {
        ret
    }

    fun f() -> std::void::Void {
        g("hello", 1, 2, 3, 4)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_variadic_mixed_generic_type, R"(
    fun g[..Ts](a: std::string::Str, ..b: Ts) -> std::void::Void {
        ret
    }

    fun f() -> std::void::Void {
        g("hello", 1, true, "world")
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_valid_variadic_given_no_args, R"(
    fun g(a: std::string::Str, ..b: std::boolean::Bool) -> std::void::Void {
        ret
    }

    fun f() -> std::void::Void {
        g("hello")
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstPostfixExpressionOperatorFunctionCallAst,
    test_invalid_variadic_mixed_arg_types,
    SppFunctionCallNoValidSignaturesError, R"(
    fun g(a: std::string::Str, ..b: std::boolean::Bool) -> std::void::Void {
        ret
    }

    fun f() -> std::void::Void {
        g("hello", 1, true, "world")
    }
)")
