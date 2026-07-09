#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTypeStatementAst,
    test_invalid_type_statement_old_type_convention_mut,
    SppSecondClassBorrowViolationError, R"(
    type MyType = &mut Bool
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTypeStatementAst,
    test_invalid_type_statement_old_type_convention_ref,
    SppSecondClassBorrowViolationError, R"(
    type MyType = &Bool
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTypeStatementAst,
    test_invalid_type_statement_unknown_old_type,
    SppIdentifierUnknownError, R"(
    type MyType = Unknown
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTypeStatementAst,
    test_invalid_type_statement_duplicate,
    SppIdentifierDuplicateError, R"(
    type MyType = Bool
    type MyType = StrView
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_simple_alias, R"(
    type MyString = StrView
    type MyBool = Bool

    fun f(a: MyString, b: MyBool) -> Void { }
    fun g() -> Void { f("hello", true) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_local_simple_alias, R"(
    fun f() -> Void {
        type MyString = StrView
        type MyBool = Bool

        let x: (MyString, MyBool)
        x = ("hello", true)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_variant, R"(
    type SomeType = StrView or Bool
    fun f(a: SomeType) -> Void { }
    fun g() -> Void { f("hello") }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_local_variant, R"(
    fun f() -> Void {
        type SomeType = StrView or Bool
        let x: SomeType
        x = "hello"
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_generics_alias, R"(
    type MyVec[T] = Vec[T]

    fun f[T](mut a: MyVec[T], replacement: T) -> Void {
        # let mut x = a.take_head()
        # x = replacement
    }

    fun g() -> Void {
        let x = Vec[StrView]()
        f(x, "test")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_local_generics_alias, R"(
    fun f() -> Void {
        type MyVec[T] = Vec[T]
        let x = MyVec[StrView]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_nested_generic_alias, R"(
    fun f[T](a: Opt[T]) -> Void { }
    fun g() -> Void {
        let x = Some(val=123)
        f(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_reduction_type_generic, R"(
    type MyVec[T] = Vec[T]

    fun f[T](a: MyVec[T]) -> Void { }
)");
