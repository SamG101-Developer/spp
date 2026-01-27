#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_free_functions,
    SppFunctionPrototypeConflictError, R"(
    fun f(a: std::boolean::Bool) -> std::void::Void { }
    fun f(a: std::boolean::Bool) -> std::void::Void { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_different_sup_blocks,
    SppFunctionPrototypeConflictError, R"(
    cls MyType { }

    sup MyType {
        fun f(a: std::boolean::Bool) -> std::void::Void { }
    }

    sup MyType {
        fun f(a: std::boolean::Bool) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_same_sup_block,
    SppFunctionPrototypeConflictError, R"(
    cls MyType { }

    sup MyType {
        fun f(a: std::boolean::Bool) -> std::void::Void { }
        fun f(a: std::boolean::Bool) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_with_optional_param_vs_no_param,
    SppFunctionPrototypeConflictError, R"(
    fun f() -> std::void::Void { }
    fun f(a: std::boolean::Bool = false) -> std::void::Void { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_with_optional_param_vs_one_param,
    SppFunctionPrototypeConflictError, R"(
    fun f(a: std::boolean::Bool) -> std::void::Void { }
    fun f(a: std::boolean::Bool, b: std::boolean::Bool = true) -> std::void::Void { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_with_variadic_param_vs_no_param,
    SppFunctionPrototypeConflictError, R"(
    fun f() -> std::void::Void { }
    fun f(..a: std::boolean::Bool) -> std::void::Void { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_with_variadic_param_vs_one_param,
    SppFunctionPrototypeConflictError, R"(
    fun f(a: std::boolean::Bool) -> std::void::Void { }
    fun f(a: std::boolean::Bool, ..b: std::boolean::Bool) -> std::void::Void { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_with_one_param_vs_optional_param,
    SppFunctionPrototypeConflictError, R"(
    fun f(a: std::boolean::Bool = false) -> std::void::Void { }
    fun f(a: std::boolean::Bool) -> std::void::Void { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_with_two_params_vs_optional_param_and_one_param,
    SppFunctionPrototypeConflictError, R"(
    fun f(a: std::string::Str, b: std::boolean::Bool = false) -> std::void::Void { }
    fun f(a: std::string::Str, b: std::boolean::Bool) -> std::void::Void { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflic_ignoring_self_variation,
    SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        fun f(&self, a: std::boolean::Bool) -> std::void::Void { }
        fun f(&mut self, a: std::boolean::Bool) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_with_same_self_and_optional_param_vs_no_param_same_sup_block,
    SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        fun f(&self, a: std::boolean::Bool = false) -> std::void::Void { }
        fun f(&self) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_with_same_self_and_optional_param_vs_no_param_different_sup_blocks,
    SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        fun f(&self, a: std::boolean::Bool = false) -> std::void::Void { }
    }
    sup A {
        fun f(&self) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_self_in_free_function,
    SppSelfParamInFreeFunctionError, R"(
    fun f(&self) -> std::void::Void { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_return_type_mut,
    SppSecondClassBorrowViolationError, R"(
    fun f() -> &mut std::void::Void { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_return_type_ref,
    SppSecondClassBorrowViolationError, R"(
    fun f() -> &std::void::Void { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_return_type_mut_via_generic_substitution,
    SppSecondClassBorrowViolationError, R"(
    fun f[T]() -> T { ret T() }

    fun g() -> std::void::Void {
        let x = f[&mut std::string::Str]()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_return_type_ref_via_generic_substitution,
    SppSecondClassBorrowViolationError, R"(
    fun f[T]() -> T { ret T() }

    fun g() -> std::void::Void {
        let x = f[&std::string::Str]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionPrototypeAst,
    test_valid_one_parameter, R"(
    fun f[T](a: T) -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionPrototypeAst,
    test_valid_no_parameters, R"(
    fun f[T]() -> std::void::Void { }
)");
