#include "../test_macros.hpp"

// The `Self` type is the "implementor" type, resolving to whichever class a `sup` block is attached
// to (including its generic arguments). These tests exercise `Self` in every position a type can be
// used. When extending a base that uses `Self`, the implementor must also spell it `Self` (it matches
// the base's `Self`, not the substituted implementor type) - this is stricter than Rust.

// --- Self in method signatures --------------------------------------------------------------------

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSelfType,
    test_valid_self_as_return_type_and_constructor, R"(
    cls A { }
    sup A {
        !public fun make() -> Self { ret Self() }
    }

    fun f() -> Void {
        let a = A::make()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSelfType,
    test_valid_self_as_parameter_type, R"(
    cls A { }
    sup A {
        !public fun eq(&self, that: &Self) -> Bool { ret true }
    }

    fun f() -> Void {
        let a1 = A()
        let a2 = A()
        let mut b = a1.eq(&a2)
        b = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSelfType,
    test_valid_self_in_nested_generic_return, R"(
    cls A { }
    sup A {
        !public fun make() -> Vec[Self] { ret Vec[Self]() }
    }

    fun f() -> Void {
        let x = A::make()
    }
)");

// --- Self:: static access -------------------------------------------------------------------------

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSelfType,
    test_valid_self_static_method_call, R"(
    cls A { }
    sup A {
        !public fun make() -> Self { ret Self() }
        !public fun make_via_self() -> Self { ret Self::make() }
    }

    fun f() -> Void {
        let a = A::make_via_self()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSelfType,
    test_valid_self_constant_access, R"(
    cls A { }
    sup A {
        !public cmp const_val: S32 = 42
        !public fun get(&self) -> S32 { ret Self::const_val }
    }

    fun f() -> Void {
        let a = A()
        let mut r = a.get()
        r = 0
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSelfType,
    test_valid_self_nested_type_access, R"(
    cls A { }
    sup A {
        !public type Inner = Bool
        !public fun get(&self) -> Self::Inner { ret true }
    }

    fun f() -> Void {
        let a = A()
        let mut b = a.get()
        b = false
    }
)");

// --- Self on a generic class ----------------------------------------------------------------------

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSelfType,
    test_valid_self_on_generic_class, R"(
    cls A[T] { }
    sup [T] A[T] {
        !public fun make() -> Self { ret Self() }
    }

    fun f() -> Void {
        let a = A[Bool]::make()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSelfType,
    test_valid_self_on_generic_class_param_type, R"(
    cls A[T] { !public val: T }
    sup [T] A[T] {
        !public fun combine(&self, that: &Self) -> Bool { ret true }
    }

    fun f() -> Void {
        let a1 = A(val=true)
        let a2 = A(val=false)
        let mut b = a1.combine(&a2)
        b = false
    }
)");

// --- Self as a generic default, resolved via extension --------------------------------------------

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSelfType,
    test_valid_self_as_generic_default_resolved_on_impl, R"(
    cls MyEq[Rhs=Self] { }
    sup [Rhs] MyEq[Rhs] {
        !public
        !abstract_method
        fun my_eq(&self, that: &Rhs) -> Bool { }
    }

    cls Point { }
    sup Point ext MyEq {
        fun my_eq(&self, that: &Self) -> Bool { ret true }
    }

    fun f() -> Void {
        let p1 = Point()
        let p2 = Point()
        let mut b = p1.my_eq(&p2)
        b = false
    }
)");

// --- Self as an explicit generic argument in an `ext` clause (the std ops pattern) ----------------

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSelfType,
    test_valid_self_as_explicit_generic_argument_in_ext, R"(
    cls MyAdd[Rhs=Self, Ret=Self] { }
    sup [Rhs, Ret] MyAdd[Rhs, Ret] {
        !public
        !abstract_method
        fun my_add(self, that: Rhs) -> Ret { }
    }

    cls Num { }
    sup Num ext MyAdd[Rhs=Self, Ret=Self] {
        fun my_add(self, that: Self) -> Self { ret Self() }
    }

    fun f() -> Void {
        let n1 = Num()
        let n2 = Num()
        let n3 = n1.my_add(n2)
    }
)");

// --- Invalid uses ---------------------------------------------------------------------------------

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestSelfType,
    test_invalid_self_type_in_free_function,
    SppIdentifierUnknownError, R"(
    fun f() -> Self { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestSelfType,
    test_invalid_superimposition_self_extension,
    SppSuperimpositionSelfExtensionError, R"(
    cls A { }
    sup A ext A { }
)");