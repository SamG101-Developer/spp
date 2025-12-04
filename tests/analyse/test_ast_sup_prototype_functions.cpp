#include "../test_macros.hpp"
import testex;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeFunctionsAst,
    test_invalid_sup_prototype_functions_unknown_type,
    SppIdentifierUnknownError, R"(
    sup A {
        fun f(&self) -> std::void::Void { }
    }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeFunctionsAst,
    test_invalid_sup_prototype_functions_unconstrained_generic_parameter_1,
    SppSuperimpositionUnconstrainedGenericParameterError, R"(
    cls Point {
        x: std::bignum::bigint::BigInt
        y: std::bignum::bigint::BigInt
    }

    sup [T] Point { }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeFunctionsAst,
    test_invalid_sup_prototype_functions_unconstrained_generic_parameter_2,
    SppSuperimpositionUnconstrainedGenericParameterError, R"(
    cls Point[T] {
        x: T
        y: T
    }

    sup [T, U] Point[T] { }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeFunctionsAst,
    test_invalid_sup_prototype_functions_optional_generic_parameter,
    SppSuperimpositionOptionalGenericParameterError, R"(
    cls Point[T] {
        x: T
        y: T
    }

    sup [T=std::boolean::Bool] Point[T] { }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    SupPrototypeFunctionsAst,
    test_valid_sup_prototype_functions_onto_generic_type, R"(
    sup [T] T {
        fun f(&self) -> std::void::Void { }
    }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeFunctionsAst,
    test_invalid_superimposition_functions_type_convention_mut,
    SppSecondClassBorrowViolationError, R"(
    cls A { }
    sup &mut A { }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeFunctionsAst,
    test_invalid_superimposition_functions_type_convention_ref,
    SppSecondClassBorrowViolationError, R"(
    cls A { }
    sup &A { }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    SupPrototypeFunctionsAst,
    test_valid_sup_prototype_functions, R"(
    cls A { }
    sup A {
        fun f(&self) -> std::void::Void { }
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    SupPrototypeFunctionsAst,
    test_valid_sup_prototype_functions_generic_fallthrough_explicit, R"(
    cls BaseClass[T] { }
    sup [T] BaseClass[T] {
        @no_impl
        fun f(&self) -> T { }
    }
    fun f() -> std::void::Void {
        let x = BaseClass[std::boolean::Bool]()
        let mut y = x.f()
        y = false
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    SupPrototypeFunctionsAst,
    test_valid_sup_prototype_functions_generic_fallthrough_implicit, R"(
    cls BaseClass[T] { a: T }
    sup [T] BaseClass[T] {
        @no_impl
        fun f(&self) -> T { }
    }
    fun f() -> std::void::Void {
        let x = BaseClass(a=false)
        let mut y = x.f()
        y = false
    }
)");;
