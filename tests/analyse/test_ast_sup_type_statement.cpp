#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupTypeStatementAst,
    test_invalid_sup_type_statement_type_mismatch,
    SppTypeMismatchError, R"(
    cls MyType { }
    sup MyType {
        type X = Str
    }

    fun f() -> Void {
        let x: MyType::X
        x = 123
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupTypeStatementAst,
    test_invalid_sup_type_statement_nested,
    SppTypeMismatchError, R"(
    cls MyType1 { }
    sup MyType1 {
        type Z = Str
    }

    cls MyType2 { }
    sup MyType2 {
        type Y = MyType1
    }

    cls MyType3 { }
    sup MyType3 {
        type X = MyType2
    }

    fun f() -> Void {
        let x: MyType3::X::Y::Z
        x = 123
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupTypeStatementAst,
    test_invalid_sup_type_statement_with_generic,
    SppTypeMismatchError, R"(
    cls MyType[T] { }
    sup [T] MyType[T] {
        type X = T
    }

    fun f() -> Void {
        let x: MyType[S32]::X
        x = Str::from("hello world")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    SupTypeStatementAst,
    test_valid_sup_type_statement, R"(
    cls MyType { }
    sup MyType {
        type X = Str
    }

    fun f() -> Void {
        let x: MyType::X
        x = Str::from("hello world")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    SupTypeStatementAst,
    test_valid_sup_type_statement_nested, R"(
    cls MyType1 { }
    sup MyType1 {
        type Z = Str
    }

    cls MyType2 { }
    sup MyType2 {
        type Y = MyType1
    }

    cls MyType3 { }
    sup MyType3 {
        type X = MyType2
    }

    fun f() -> Void {
        let x: MyType3::X::Y::Z
        x = Str::from("hello world")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    SupTypeStatementAst,
    test_valid_sup_type_statement_with_generic, R"(
    cls MyType[T] { }
    sup [T] MyType[T] {
        type X = T
    }

    fun f() -> Void {
        let x: MyType[S32]::X
        x = 123
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupTypeStatementAst,
    test_invalid_sup_type_statement_constraint_mismatch,
    SppGenericConstraintError, R"(
    cls A { }
    cls B[T: A] { }
    cls MyType { }
    sup MyType {
        type X[U] = B[U]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    SupTypeStatementAst,
    test_valid_sup_type_statement_constraint, R"(
    cls A { }
    cls B[T: A] { }
    cls MyType { }
    sup MyType {
        type X[U: A] = B[U]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupTypeStatementAst,
    test_invalid_sup_type_statement_unknown_old_type,
    SppIdentifierUnknownError, R"(
    cls MyType { }
    sup MyType {
        type X = Unknown
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupTypeStatementAst,
    test_invalid_sup_type_statement_convention_on_old_type,
    SppSecondClassBorrowViolationError, R"(
    cls MyType { }
    sup MyType {
        type X = &mut Bool
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    SupTypeStatementAst,
    test_valid_sup_type_statement_variant, R"(
    cls MyType { }
    sup MyType {
        type X = Str or Bool
    }

    fun f() -> Void {
        let x: MyType::X
        x = Str::from("hello world")
    }
)");
