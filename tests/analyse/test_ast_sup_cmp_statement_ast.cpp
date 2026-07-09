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
    SppMovingEscapingBorrowedMemoryError, R"(
    cls MyType { }
    sup MyType {
        !public cmp n: (StrView, StrView) = ("hello world", "hello world")
    }

    fun f() -> Void {
        let mut local_n = MyType::n
        local_n = ("hello world", "hello world")
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
