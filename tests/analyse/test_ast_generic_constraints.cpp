#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericConstraints,
    test_invalid_function_constraint_mismatch,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A { }
    fun g[T: A](t: T) -> Void { }

    fun f() -> Void {
        g(123)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericConstraints,
    test_invalid_function_constraint_multiple_mismatch,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A { }
    cls B { }

    cls C { }
    sup C ext A { }
    fun g[T: A & B](t: T) -> Void { }

    fun f() -> Void {
        let c = C()
        g(c)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericConstraints,
    test_valid_function_constraint, R"(
    cls A { }
    fun g[T: A](t: T) -> Void {
        std::mem::ops::drop(t)
    }

    fun f() -> Void {
        let a = A()
        g(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericConstraints,
    test_valid_function_constraint_multiple, R"(
    cls A { }
    cls B { }

    cls C { }
    sup C ext A { }
    sup C ext B { }

    fun g[T: A & B](t: T) -> Void {
        std::mem::ops::drop(t)
    }

    fun f() -> Void {
        let c = C()
        g(c)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericConstraints,
    test_invalid_class_constraint_mismatch,
    SppGenericConstraintError, R"(
    cls A { }
    cls B[T: A] { }

    fun f() -> Void {
        let b = B[U32]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericConstraints,
    test_invalid_class_constraint_multiple_mismatch,
    SppGenericConstraintError, R"(
    cls A { }
    cls B { }

    cls C { }
    sup C ext A { }
    cls D[T: A & B] { }

    fun f() -> Void {
        let c = C()
        let d = D[C]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericConstraints,
    test_valid_class_constraint, R"(
    cls A { }
    cls B[T: A] { }

    fun f() -> Void {
        let a = A()
        let b = B[A]()
        std::mem::ops::drop(a)
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericConstraints,
    test_valid_class_constraint_multiple, R"(
    cls A { }
    cls B { }

    cls C { }
    sup C ext A { }
    sup C ext B { }
    cls D[T: A & B] { }

    fun f() -> Void {
        let c = C()
        let d = D[C]()
        std::mem::ops::drop(c)
        std::mem::ops::drop(d)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericConstraints,
    test_invalid_alias_constraint_mismatch,
    SppGenericConstraintError, R"(
    cls A { }
    cls B[T: A] { }
    type C[U] = B[U]
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericConstraints,
    test_valid_alias_constraint, R"(
    cls A { }
    cls B[T: A] { }
    type C[U: A] = B[U]
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericConstraints,
    test_invalid_sup_constraint_mismatch,
    SppGenericConstraintError, R"(
    cls A { }
    cls B[T: A] { }
    sup [X] B[X] { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericConstraints,
    test_valid_sup_constraint, R"(
    cls A { }
    cls B[T: A] { }
    sup [X: A] B[X] { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericConstraints,
    test_invalid_ext_constraint_mismatch,
    SppGenericConstraintError, R"(
    cls A { }
    cls B[T: A] { }
    cls C { }
    sup [X] C ext B[X] { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericConstraints,
    test_valid_ext_constraint, R"(
    cls A { }
    cls B[T: A] { }
    cls C { }

    sup [X: A] C ext B[X] { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericConstraints,
    test_invalid_sup_function_for_constraint_mismatch,
    SppIdentifierUnknownError, R"(
    cls A[T] { }

    sup [T: Copy] A[T] {
        !public fun my_function(&self) -> Void { }
    }

    fun f() -> Void {
        let a = A[Str]()
        a.my_function()  # invalid; Str is not Copy
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericConstraints,
    test_valid_sup_function_for_constraint,
    R"(
    cls A[T] { }

    sup [T: Copy] A[T] {
        !public fun my_function(&self) -> Void { }
    }

    fun f() -> Void {
        let a = A[U32]()
        a.my_function()  # valid; U32 is Copy
        std::mem::ops::drop(a)
    }
)");

// A variadic parameter stands for however many arguments were left, so its constraint describes each of them rather
// than the pack they were collected into. Nothing exercised that path before: every constraint test above binds one
// argument to one parameter, and the variadic case was read as a question about the tuple holding the pack - which a
// "sup [..Ts: Copy] Tup[Ts] ext Copy" answers with the very impl the constraint exists to gate.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericConstraints,
    test_valid_variadic_constraint_all_satisfy, R"(
    cls A[..Ts] { }

    sup [..Ts: Copy] A[Ts] {
        !public fun my_function(&self) -> Void { }
    }

    fun f() -> Void {
        let a = A[U32, Bool]()
        a.my_function()
        std::mem::ops::drop(a)
    }
)");

// One element that does not satisfy it is enough to reject the pack. Declared on the class rather than on a "sup",
// because an unsatisfied "sup" constraint means that superimposition simply does not apply - the method is then not
// found, which is a weaker thing to assert than the constraint itself being enforced.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericConstraints,
    test_invalid_variadic_constraint_one_element_fails,
    SppGenericConstraintError, R"(
    cls NotCopy { }
    cls A[..Ts: Copy] { }

    fun f() -> Void {
        let a = A[U32, NotCopy]()
        std::mem::ops::drop(a)
    }
)");

// ...including when it is the only element, which is the shape a single-argument pack takes, and the one most easily
// confused with the bare argument the check used to be handed.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericConstraints,
    test_invalid_variadic_constraint_single_element_fails,
    SppGenericConstraintError, R"(
    cls NotCopy { }
    cls A[..Ts: Copy] { }

    fun f() -> Void {
        let a = A[NotCopy]()
        std::mem::ops::drop(a)
    }
)");

// An empty pack has nothing to violate the constraint, so it satisfies it. Written against a function, because that
// is where an empty pack arises on its own - a call that gives the variadic parameter no arguments at all.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericConstraints,
    test_valid_variadic_constraint_empty_pack, R"(
    fun g[..Ts: Copy](..a: Ts) -> Void { }

    fun f() -> Void {
        g()
    }
)");

// A variadic function generic is the same rule on the call side.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericConstraints,
    test_invalid_variadic_function_constraint_one_element_fails,
    SppFunctionCallNoValidSignaturesError, R"(
    cls NotCopy { }

    fun g[..Ts: Copy](..a: Ts) -> Void { }

    fun f() -> Void {
        g(1, NotCopy())
    }
)");
