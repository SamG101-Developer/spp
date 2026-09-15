#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  SupTypeStatementAst,
  test_invalid_sup_type_statement_type_mismatch,
  SppTypeMismatchError, R"(
    cls MyType { }
    sup MyType {
        !public type X = Str
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
        !public type Z = Str
    }

    cls MyType2 { }
    sup MyType2 {
        !public type Y = MyType1
    }

    cls MyType3 { }
    sup MyType3 {
        !public type X = MyType2
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
        !public type X = T
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
        !public type X = Str
    }

    fun f() -> Void {
        let x: MyType::X
        x = Str::from("hello world")
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  SupTypeStatementAst,
  test_valid_sup_type_statement_nested, R"(
    cls MyType1 { }
    sup MyType1 {
        !public type Z = Str
    }

    cls MyType2 { }
    sup MyType2 {
        !public type Y = MyType1
    }

    cls MyType3 { }
    sup MyType3 {
        !public type X = MyType2
    }

    fun f() -> Void {
        let x: MyType3::X::Y::Z
        x = Str::from("hello world")
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  SupTypeStatementAst,
  test_valid_sup_type_statement_with_generic, R"(
    cls MyType[T] { }
    sup [T] MyType[T] {
        !public type X = T
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
        !public type X[U] = B[U]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  SupTypeStatementAst,
  test_valid_sup_type_statement_constraint, R"(
    cls A { }
    cls B[T: A] { }
    cls MyType { }
    sup MyType {
        !public type X[U: A] = B[U]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  SupTypeStatementAst,
  test_invalid_sup_type_statement_unknown_old_type,
  SppIdentifierUnknownError, R"(
    cls MyType { }
    sup MyType {
        !public type X = Unknown
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  SupTypeStatementAst,
  test_invalid_sup_type_statement_convention_on_old_type,
  SppSecondClassBorrowViolationError, R"(
    cls MyType { }
    sup MyType {
        !public type X = &mut Bool
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  SupTypeStatementAst,
  test_valid_sup_type_statement_variant, R"(
    cls MyType { }
    sup MyType {
        !public type X = Str or Bool
    }

    fun f() -> Void {
        let x: MyType::X
        x = Str::from("hello world")
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  SupTypeStatementAstGenericSelfClass,
  test_valid_alias_of_the_sups_own_generic_class, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        type Mine = Box[T]
        !public fun m(self) -> Mine { ret self }
    }
    fun f() -> Void {
        let b: Box[S32] = Box(v=1).m()
    }
)");

// Todo: red - "Vec[Box[T]]" aliased inside Box's own generic sup nests without end: minting "Vec[T=Box[T]]" substitutes
// its member types, and "Box[T]" is re-read at each level through the binding it is part of, so it grows a level each
// time. It now stops with E109 (generic instantiation depth) instead of overflowing the stack. The alias itself is
// finite ("Vec[Box[S32]]" in "Box[S32]"'s sup); an open instance minted in a template should not have its members
// substituted eagerly. The "Self" form in TestSelfTypePositionsGeneric is the same bug.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  SupTypeStatementAstGenericSelfClass,
  test_valid_alias_of_a_type_holding_the_sups_own_generic_class, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        type Many = Vec[Box[T]]
        !public fun m(&self) -> Many { ret Many::new() }
    }
    fun f() -> Void {
        let b = Box(v=1)
        let v: Vec[Box[S32]] = b.m()
        std::mem::ops::drop(v)
    }
)");
