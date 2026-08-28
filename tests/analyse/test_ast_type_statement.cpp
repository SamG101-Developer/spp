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
    type MyString = Str
    type MyBool = Bool

    fun f(a: MyString, b: MyBool) -> Void {
        std::mem::ops::drop(a)
    }
    fun g() -> Void { f(Str::from("hello"), true) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_local_simple_alias, R"(
    fun f() -> Void {
        type MyString = Str
        type MyBool = Bool

        let x: (MyString, MyBool)
        x = (Str::from("hello"), true)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_variant, R"(
    type SomeType = Str or Bool
    fun f(a: SomeType) -> Void { }
    fun g() -> Void { f(Str::from("hello")) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_local_variant, R"(
    fun f() -> Void {
        type SomeType = Str or Bool
        let x: SomeType
        x = Str::from("hello")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_generics_alias, R"(
    type MyVec[T] = Vec[T]

    fun f[T](mut a: MyVec[T], replacement: T) -> Void {
        # let mut x = a.take_head()
        # x = replacement
        std::mem::ops::drop(replacement)
        std::mem::ops::drop(a)
    }

    fun g() -> Void {
        let x = Vec[Str]()
        f(x, Str::from("test"))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_local_generics_alias, R"(
    fun f() -> Void {
        type MyVec[T] = Vec[T]
        let x = MyVec[Str]()
        std::mem::ops::drop(x)
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

    fun f[T](a: MyVec[T]) -> Void {
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_comp_param_alias_to_foreign_module, R"(
    type HeapArr[T, cmp n: USize] = Single[Arr[T, n]]

    fun f(a: HeapArr[Bool, 4_uz]) -> Void {
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_constrained_param_alias_to_foreign_module, R"(
    type CopyBox[T: Copy] = Single[T]

    fun f(a: CopyBox[Bool]) -> Void {
        std::mem::ops::drop(a)
    }
)");
