#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_free_functions,
    SppFunctionPrototypeConflictError, R"(
    fun f(a: Bool) -> Void { }
    fun f(a: Bool) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_different_sup_blocks,
    SppFunctionPrototypeConflictError, R"(
    cls MyType { }

    sup MyType {
        fun f(a: Bool) -> Void { }
    }

    sup MyType {
        fun f(a: Bool) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_same_sup_block,
    SppFunctionPrototypeConflictError, R"(
    cls MyType { }

    sup MyType {
        fun f(a: Bool) -> Void { }
        fun f(a: Bool) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_with_optional_param_vs_no_param,
    SppFunctionPrototypeConflictError, R"(
    fun f() -> Void { }
    fun f(a: Bool = false) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_with_optional_param_vs_one_param,
    SppFunctionPrototypeConflictError, R"(
    fun f(a: Bool) -> Void { }
    fun f(a: Bool, b: Bool = true) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_with_variadic_param_vs_no_param,
    SppFunctionPrototypeConflictError, R"(
    fun f() -> Void { }
    fun f(..a: Bool) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_with_variadic_param_vs_one_param,
    SppFunctionPrototypeConflictError, R"(
    fun f(a: Bool) -> Void { }
    fun f(a: Bool, ..b: Bool) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_with_one_param_vs_optional_param,
    SppFunctionPrototypeConflictError, R"(
    fun f(a: Bool = false) -> Void { }
    fun f(a: Bool) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_with_two_params_vs_optional_param_and_one_param,
    SppFunctionPrototypeConflictError, R"(
    fun f(a: Str, b: Bool = false) -> Void { }
    fun f(a: Str, b: Bool) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflic_ignoring_self_variation,
    SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        fun f(&self, a: Bool) -> Void { }
        fun f(&mut self, a: Bool) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_with_same_self_and_optional_param_vs_no_param_same_sup_block,
    SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        fun f(&self, a: Bool = false) -> Void { }
        fun f(&self) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_conflict_with_same_self_and_optional_param_vs_no_param_different_sup_blocks,
    SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        fun f(&self, a: Bool = false) -> Void { }
    }
    sup A {
        fun f(&self) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_self_in_free_function,
    SppSelfIdentifierInvalidContextError, R"(
    fun f(&self) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_return_type_mut,
    SppSecondClassBorrowViolationError, R"(
    fun f() -> &mut Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_return_type_ref,
    SppSecondClassBorrowViolationError, R"(
    fun f() -> &Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_return_type_mut_via_generic_substitution,
    SppSecondClassBorrowViolationError, R"(
    fun f[T]() -> T { ret T() }

    fun g() -> Void {
        let x = f[&mut Str]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionPrototypeAst,
    test_invalid_return_type_ref_via_generic_substitution,
    SppSecondClassBorrowViolationError, R"(
    fun f[T]() -> T { ret T() }

    fun g() -> Void {
        let x = f[&Str]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionPrototypeAst,
    test_valid_one_parameter, R"(
    fun f[T](a: T) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionPrototypeAst,
    test_valid_no_parameters, R"(
    fun f[T]() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionPrototypeAst,
    test_valid_return_type_self_from_self_method, R"(
    cls A { }
    sup A {
        fun f(self) -> Self { ret self }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionPrototypeAst,
    test_valid_return_type_self_from_static_method, R"(
    cls A { }
    sup A {
        fun new() -> Self { ret A() }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionPrototypeAst,
    test_valid_static_method_no_self, R"(
    cls A { }
    sup A {
        fun f() -> Void { }
    }
)");
