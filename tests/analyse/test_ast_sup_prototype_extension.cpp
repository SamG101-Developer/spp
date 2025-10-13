#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeExtensionAst,
    test_invalid_sup_prototype_functions_unknown_type,
    SppIdentifierUnknownError, R"(
    sup A {
        fun f(&self) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeExtensionAst,
    test_invalid_sup_prototype_functions_unknown_supertype,
    SppIdentifierUnknownError, R"(
    sup std::boolean::Bool ext A {
        fun f(&self) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    SupPrototypeExtensionAst,
    test_valid_superimposition_extension_generic_name, R"(
    cls A { }
    sup [T] T ext A { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    SupPrototypeExtensionAst,
    DISABLED_test_valid_superimposition_extension_generic_name_more_complex, R"(
    cls CanBorrowRef[T] { }
    sup [T] CanBorrowRef[T] {
        cor borrow_ref(&self) -> std::generator::Gen[&T] {
            gen self
        }
    }

    cls CanBorrowMut[T] { }
    sup [T] CanBorrowMut[T] {
        cor borrow_mut(&mut self) -> std::generator::Gen[&mut T] {
            gen self
        }
    }

    sup [T] T ext CanBorrowRef[T] { }
    sup [T] T ext CanBorrowMut[T] { }

    fun f() -> std::void::Void {
        let x = 123
        let y = x.borrow_ref()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeExtensionAst,
    test_invalid_superimposition_extension_generic_superclass,
    SppGenericTypeInvalidUsageError, R"(
    sup [T] std::vector::Vec[T] ext T { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeExtensionAst,
    test_invalid_superimposition_extension_unconstrained_generic,
    SppSuperimpositionUnconstrainedGenericParameterError, R"(
    sup [T] std::boolean::Bool ext std::number::S32 { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeExtensionAst,
    test_invalid_superimposition_extension_duplication_superclass,
    SppSuperimpositionDoubleExtensionError, R"(
    sup std::boolean::Bool ext std::string::Str { }
    sup std::boolean::Bool ext std::string::Str { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeExtensionAst,
    test_invalid_superimposition_extension_cyclic_extension,
    SppSuperimpositionCyclicExtensionError, R"(
    sup std::boolean::Bool ext std::string::Str { }
    sup std::string::Str ext std::boolean::Bool { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeExtensionAst,
    test_invalid_superimposition_extension_self_extension,
    SppSuperimpositionSelfExtensionError, R"(
    cls A { }
    sup A ext A { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeExtensionAst,
    test_invalid_superimposition_extension_invalid_override_method_1,
    SppSuperimpositionExtensionMethodInvalidError, R"(
    cls A { }
    sup A {
        fun f(&self) -> std::void::Void { }
    }

    cls B { }
    sup B ext A {
        fun g(&self) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeExtensionAst,
    test_invalid_superimposition_extension_invalid_override_method_2,
    SppSuperimpositionExtensionMethodInvalidError, R"(
    cls A { }
    sup A {
        fun f(&self) -> std::void::Void { }
    }

    cls B { }
    sup B ext A {
        fun f(&mut self) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeExtensionAst,
    test_invalid_superimposition_extension_invalid_override_method_3,
    SppSuperimpositionExtensionMethodInvalidError, R"(
    cls A { }
    sup A {
        fun f(&self) -> std::void::Void { }
    }

    cls B { }
    sup B ext A {
        fun f(&self, x: std::boolean::Bool = true) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeExtensionAst,
    test_invalid_superimposition_extension_invalid_override_method_4,
    SppSuperimpositionExtensionMethodInvalidError, R"(
    cls A { }
    sup A {
        fun f(&self) -> std::void::Void { }
    }

    cls B { }
    sup B ext A {
        fun f(&self) -> std::boolean::Bool { ret true }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeExtensionAst,
    test_invalid_superimposition_extension_non_virtual_method_override,
    SppSuperimpositionExtensionNonVirtualMethodOverriddenError, R"(
    cls A { }
    sup A {
        fun f(&self) -> std::void::Void { }
    }

    cls B { }
    sup B ext A {
        fun f(&self) -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeExtensionAst,
    test_invalid_superimposition_extension_invalid_type_statement,
    SppSuperimpositionExtensionTypeStatementInvalidError, R"(
    cls A { }

    cls B { }
    sup B ext A {
        type X = std::string::Str
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeExtensionAst,
    test_invalid_superimposition_extension_invalid_cmp_statement,
    SppSuperimpositionExtensionCmpStatementInvalidError, R"(
    cls A { }

    cls B { }
    sup B ext A {
        cmp x: std::string::Str = "hello world"
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeExtensionAst,
    test_invalid_superimposition_extension_type_convention_mut,
    SppSecondClassBorrowViolationError, R"(
    cls A { }
    sup &mut A ext std::number::S32 { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeExtensionAst,
    test_invalid_superimposition_extension_type_convention_ref,
    SppSecondClassBorrowViolationError, R"(
    cls A { }
    sup &A ext std::number::S32 { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeExtensionAst,
    test_invalid_superimposition_extension_supertype_convention_mut,
    SppSecondClassBorrowViolationError, R"(
    cls A { }
    sup A ext &mut std::number::S32 { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupPrototypeExtensionAst,
    test_invalid_superimposition_extension_supertype_convention_ref,
    SppSecondClassBorrowViolationError, R"(
    cls A { }
    sup A ext &std::number::S32 { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    SupPrototypeExtensionAst,
    test_valid_superimposition_extension_generic_variants, R"(
    cls BaseClass[T] { }

    cls A { }
    sup A ext BaseClass[std::number::S32] { }
    sup A ext BaseClass[std::boolean::Bool] { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    SupPrototypeExtensionAst,
    test_valid_superimposition_extension_stateful, R"(
    cls A { a: std::number::S32 }
    cls B { b: std::number::S32 }

    sup A {
        @virtual_method
        fun f(mut self) -> std::void::Void {
            self.a = 100
        }
    }

    sup B ext A {
        fun f(mut self) -> std::void::Void {
            self.a = self.b
        }
    }

    fun f() -> std::void::Void {
        let b = B(b=200)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    SupPrototypeExtensionAst,
    test_valid_superimposition_extension_generics_1, R"(
    cls A[T] { a: T }
    cls B[T] { b: T }

    sup [T] A[T] {
        @virtual_method
        fun f(mut self) -> std::void::Void { }
    }

    sup [T] B[T] ext A[T] {
        fun f(mut self) -> std::void::Void {
            self.a = self.b
        }
    }

    fun f() -> std::void::Void {
        let b = B(b=100)
    }
)");
