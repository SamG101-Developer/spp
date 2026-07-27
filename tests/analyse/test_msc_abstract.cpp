#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAbstract,
    test_abstract_type_as_superimposition_target, R"(
    cls A { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    fun test() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAbstract,
    test_abstract_type_as_superclass, R"(
    cls A { }
    cls B { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    sup B ext A {
        fun f(&self) -> Void { }
    }

    fun test() -> Void {
        let b = B()
        b.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAbstract,
    test_abstract_type_as_generic_constraint, R"(
    cls A { }
    cls B { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    sup B ext A {
        fun f(&self) -> Void { }
    }

    fun g[T: A](t: &T) -> Void {
        t.f()
    }

    fun test() -> Void {
        let b = B()
        g(&b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAbstract,
    test_fully_implemented_derived_type_is_concrete, R"(
    cls A { }
    cls B { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }

        !public
        !abstract_method
        fun g(&self) -> Void { }
    }

    sup B ext A {
        fun f(&self) -> Void { }
        fun g(&self) -> Void { }
    }

    fun h(b: B) -> Void { }

    fun test() -> Void {
        let b = B()
        h(b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAbstract,
    test_implementation_in_separate_sup_block, R"(
    cls A { }
    cls B { }

    sup B ext A {
        fun f(&self) -> Void { }
    }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    fun test() -> Void {
        let b = B()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAbstract,
    test_overload_does_not_implement_abstract_method,
    SppAbstractTypeUseError, R"(
    cls A { }
    cls B { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }

        !public
        !virtual_method
        fun f(&self, a: S32) -> Void { }
    }

    sup B ext A {
        fun f(&self, a: S32) -> Void { }
    }

    fun test() -> Void {
        let b = B()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAbstract,
    test_derived_type_implementing_nothing_is_abstract,
    SppAbstractTypeUseError, R"(
    cls A { }
    cls B { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    sup B ext A { }

    fun test() -> Void {
        let b = B()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAbstract,
    test_partially_implemented_derived_type_is_abstract,
    SppAbstractTypeUseError, R"(
    cls A { }
    cls B { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }

        !public
        !abstract_method
        fun g(&self) -> Void { }
    }

    sup B ext A {
        fun f(&self) -> Void { }
    }

    fun test() -> Void {
        let b = B()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAbstract,
    test_abstractness_resolved_down_a_chain, R"(
    cls A { }
    cls B { }
    cls C { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }

        !public
        !abstract_method
        fun g(&self) -> Void { }
    }

    sup B ext A {
        fun f(&self) -> Void { }
    }

    sup C ext B {
        fun g(&self) -> Void { }
    }

    fun test() -> Void {
        let c = C()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAbstract,
    test_abstract_type_object_initializer,
    SppAbstractTypeUseError, R"(
    cls A { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    fun test() -> Void {
        let a = A()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAbstract,
    test_abstract_type_as_parameter_type,
    SppAbstractTypeUseError, R"(
    cls A { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    fun g(a: A) -> Void { }

    fun test() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAbstract,
    test_abstract_type_as_return_type,
    SppAbstractTypeUseError, R"(
    cls A { }
    cls B { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    sup B {
        !public
        !abstract_method
        fun g(&self) -> A { }
    }

    fun test() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAbstract,
    test_abstract_type_as_class_attribute_type,
    SppAbstractTypeUseError, R"(
    cls A { }
    cls B { a: A }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    fun test() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAbstract,
    test_abstract_type_as_local_variable_type,
    SppAbstractTypeUseError, R"(
    cls A { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    fun test() -> Void {
        let a: A
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAbstract,
    test_abstract_type_as_generic_argument_no_usage, R"(
    cls A { }
    cls B[T] { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    fun g(b: B[A]) -> Void { }

    fun test() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAbstract,
    test_borrow_of_abstract_type_as_parameter_type,
    SppAbstractTypeUseError, R"(
    cls A { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    fun g(a: &A) -> Void { }

    fun test() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAbstract,
    test_alias_of_abstract_type_is_abstract,
    SppAbstractTypeUseError, R"(
    cls A { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    type AliasA = A

    fun test() -> Void {
        let a = AliasA()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAbstract,
    test_alias_of_propagated_abstract_type_is_abstract,
    SppAbstractTypeUseError, R"(
    cls A { }
    cls B { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    sup B ext A { }

    type AliasB = B

    fun test() -> Void {
        let b = AliasB()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAbstract,
    test_alias_of_concrete_type_is_usable, R"(
    cls A { }
    cls B { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    sup B ext A {
        fun f(&self) -> Void { }
    }

    type AliasB = B

    fun test() -> Void {
        let b = AliasB()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAbstract,
    test_generic_alias_of_abstract_type_is_abstract,
    SppAbstractTypeUseError, R"(
    cls A[T] { }

    sup [T] A[T] {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    type AliasA[T] = A[T]

    fun test() -> Void {
        let a = AliasA[S32]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAbstract,
    test_generic_abstract_type_substitution_is_abstract,
    SppAbstractTypeUseError, R"(
    cls A[T] { }

    sup [T] A[T] {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    fun test() -> Void {
        let a = A[S32]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAbstract,
    test_generic_derived_type_implementing_abstract_method_is_concrete, R"(
    cls A[T] { }
    cls B[T] { }

    sup [T] A[T] {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    sup [T] B[T] ext A[T] {
        fun f(&self) -> Void { }
    }

    fun test() -> Void {
        let b = B[S32]()
        b.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAbstract,
    test_unsubstituted_generic_parameter_is_not_abstract, R"(
    cls A { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    fun g[T](t: T) -> T { ret t }

    fun test() -> Void {
        let x = g(123)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAbstract,
    test_abstract_type_substituted_into_function_generic,
    SppAbstractTypeUseError, R"(
    cls A { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    fun g[T]() -> Void { }

    fun test() -> Void {
        g[A]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAbstract,
    test_abstract_type_substituted_into_class_generic,
    SppAbstractTypeUseError, R"(
    cls A { }
    cls B[T] { t: T }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    fun test() -> Void {
        let b = B[A]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAbstract,
    test_abstract_type_as_optional_generic_default,
    SppAbstractTypeUseError, R"(
    cls A { }
    cls B[T = A] { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    fun test() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAbstract,
    test_abstract_constraint_with_concrete_generic_default, R"(
    cls A { }
    cls B { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> Void { }
    }

    sup B ext A {
        fun f(&self) -> Void { }
    }

    cls C[T: A = B] { }

    fun test() -> Void {
        let c = C()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAbstract,
    test_implementation_widening_return_type,
    SppSuperimpositionExtensionMethodInvalidError, R"(
    cls A { }
    cls B { }

    sup A {
        !public
        !abstract_method
        fun f(&self) -> S32 { }
    }

    sup B ext A {
        fun f(&self) -> S32 or Bool { ret 123 }
    }

    fun test() -> Void {
        let b = B()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAbstract,
    test_subroutine_does_not_implement_abstract_coroutine,
    SppSuperimpositionExtensionMethodInvalidError, R"(
    cls A { }
    cls B { }

    sup A {
        !public
        !abstract_method
        cor f(&self) -> Gen[S32] { }
    }

    sup B ext A {
        fun f(&self) -> Gen[S32] { }
    }

    fun test() -> Void {
        let b = B()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAbstract,
    test_virtual_method_does_not_make_type_abstract, R"(
    cls A { }

    sup A {
        !public
        !virtual_method
        fun f(&self) -> Void { }
    }

    fun test() -> Void {
        let a = A()
        a.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAbstract,
    test_plain_type_is_not_abstract, R"(
    cls A { }

    sup A {
        !public
        fun f(&self) -> Void { }
    }

    fun test() -> Void {
        let a = A()
        a.f()
    }
)");
