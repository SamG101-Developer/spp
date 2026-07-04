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
    fun g[T: A](t: T) -> Void { }

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

    fun g[T: A & B](t: T) -> Void { }

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
    }
)");
