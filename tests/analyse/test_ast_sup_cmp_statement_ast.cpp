#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupCmpStatementAst,
    test_invalid_type_mismatch,
    SppTypeMismatchError, R"(
    cls MyType { }
    sup MyType {
        !public cmp n: USize = 123_uz
    }

    fun f() -> Void {
        let mut local_n = MyType::n
        local_n = "hello world"
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupCmpStatementAst,
    test_invalid_direct_type_mismatch,
    SppTypeMismatchError, R"(
    cls MyType { }
    sup MyType {
        !public cmp n: USize = "hello world"
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupCmpStatementAst,
    test_invalid_duplicate_name_in_sup,
    SppIdentifierDuplicateError, R"(
    cls MyType { }
    sup MyType {
        !public cmp n: USize = 123_uz
        !public cmp n: USize = 456_uz
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupCmpStatementAst,
    test_invalid_moving_non_copy_cmp,
    SppMovingComptimeConstantMemoryError, R"(
    cls X { }

    cls MyType { }
    sup MyType {
        !public cmp n: (X, X) = (X(), X())
    }

    fun f() -> Void {
        let mut local_n = MyType::n
        local_n = (X(), X())
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupCmpStatementAst,
    test_invalid_type_mismatch_with_generic,
    SppTypeMismatchError, R"(
    cls MyType[T, cmp m: T] { }
    sup [T, cmp m: T] MyType[T, m] {
        !public cmp n: T = m
    }

    fun f() -> Void {
        let mut x = MyType[USize, 123_uz]::n
        x = "hello world"
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupCmpStatementAst,
    test_valid_simple, R"(
    cls MyType { }
    sup MyType {
        !public cmp n: USize = 123_uz
    }

    fun f() -> Void {
        let mut local_n = MyType::n
        local_n = 456_uz
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupCmpStatementAst,
    test_invalid_with_generic_move,
    SppMovingComptimeConstantMemoryError, R"(
    cls MyType[T, cmp m: T] { }
    sup [T, cmp m: T] MyType[T, m] {
        !public cmp n: T = m
    }

    fun f() -> Void {
        let mut x = MyType[StrView, "123"]::n
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupCmpStatementAst,
    test_valid_with_generic_copy, R"(
    cls MyType[T, cmp m: T] { }
    sup [T: Copy, cmp m: T] MyType[T, m] {
        !public cmp n: T = m
    }

    fun f() -> Void {
        let mut x = MyType[S32, 123]::n
    }
)");

// Todo: red - a generic sup "cmp" of a class type gets a global typed with the unbound "Unit[T=T]", which LLVM
// rejects as unsized.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  SupCmpStatementGeneric,
  test_valid_class_typed_cmp_in_a_generic_sup, R"(
    cls Unit[T] { }
    sup [T] Unit[T] ext std::copy::Copy { }
    sup [T] Unit[T] {
        !public cmp empty: Unit[T] = Unit[T]()
    }
    fun f() -> Void {
        let u: Unit[S32] = Unit[S32]::empty
    }
)");
