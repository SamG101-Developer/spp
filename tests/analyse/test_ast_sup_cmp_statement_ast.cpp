#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupCmpStatementAst,
    test_invalid_type_mismatch,
    SppTypeMismatchError, R"(
    cls MyType { }
    sup MyType {
        cmp n: std::number::USize = 123_uz
    }

    fun f() -> std::void::Void {
        let mut local_n = MyType::n
        local_n = "hello world"
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupCmpStatementAst,
    test_invalid_moving_non_copy_cmp,
    SppMoveFromPinnedMemoryError, R"(
    cls MyType { }
    sup MyType {
        cmp n: std::string::Str = "hello world"
    }

    fun f() -> std::void::Void {
        let mut local_n = MyType::n
        local_n = "hello world"
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupCmpStatementAst,
    test_invalid_type_mismatch_with_generic,
    SppTypeMismatchError, R"(
    cls MyType[T, cmp m: T] { }
    sup [T, cmp m: T] MyType[T, m] {
        cmp n: T = m
    }

    fun f() -> std::void::Void {
        let mut x = MyType[std::number::USize, 123_uz]::n
        x = "hello world"
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstSupCmpStatementAst,
    test_valid_simple, R"(
    cls MyType { }
    sup MyType {
        cmp n: std::number::USize = 123_uz
    }

    fun f() -> std::void::Void {
        let mut local_n = MyType::n
        local_n = 456_uz
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupCmpStatementAst,
    test_invalid_with_generic_move,
    SppMoveFromPinnedMemoryError, R"(
    cls MyType[T, cmp m: T] { }
    sup [T, cmp m: T] MyType[T, m] {
        cmp n: T = m
    }

    fun f() -> std::void::Void {
        let mut x = MyType[std::string::Str, "123"]::n
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstSupCmpStatementAst,
    test_invalid_with_generic_copy,
    SppMoveFromPinnedMemoryError, R"(
    cls MyType[T, cmp m: T] { }
    sup [T, cmp m: T] MyType[T, m] {
        cmp n: T = m
    }

    fun f() -> std::void::Void {
        let mut x = MyType[std::number::U32, 123]::n
    }
)");
