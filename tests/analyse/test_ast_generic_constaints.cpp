#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericConstraints,
    test_invalid_function_constraint_mismatch,
    SppGenericConstraintError, R"(
    cls A { }
    fun g[T: A](t: T) -> std::void::Void { }

    fun f() -> std::void::Void {
        g(123)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstGenericConstraints,
    test_invalid_function_constraint_multiple_mismatch,
    SppGenericConstraintError, R"(
    cls A { }
    cls B { }

    cls C { }
    sup C ext A { }
    fun g[T: A & B](t: T) -> std::void::Void { }

    fun f() -> std::void::Void {
        let c = C()
        g(c)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericConstraints,
    test_valid_function_constraint, R"(
    cls A { }
    fun g[T: A](t: T) -> std::void::Void { }

    fun f() -> std::void::Void {
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

    fun g[T: A & B](t: T) -> std::void::Void { }

    fun f() -> std::void::Void {
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

    fun f() -> std::void::Void {
        let b = B[std::number::U32]()
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

    fun f() -> std::void::Void {
        let c = C()
        let d = D[C]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstGenericConstraints,
    test_valid_class_constraint, R"(
    cls A { }
    cls B[T: A] { }

    fun f() -> std::void::Void {
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

    fun f() -> std::void::Void {
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
