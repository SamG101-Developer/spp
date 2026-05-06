#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryMoveFromMutBorrowedCtx,
    test_invalid_memory_moved_from_borrowed_context,
    SppMoveFromBorrowedMemoryError, R"(
    cls T { }

    cls Point {
        !public x: T
        !public y: T
    }

    fun f(p: &mut Point) -> std::void::Void {
        let x = p.x
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryMoveFromMutBorrowedCtx,
    test_invalid_memory_moved_from_borrowed_context_with_symbol_alias,
    SppMoveFromBorrowedMemoryError, R"(
    cls T { }

    cls Point {
        !public x: T
        !public y: T
    }

    fun f(p: &mut Point) -> std::void::Void {
        let q = p
        let x = q.x
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryMoveFromMutBorrowedCtx,
    test_invalid_memory_moved_from_borrowed_context_nested,
    SppMoveFromBorrowedMemoryError, R"(
    cls U { }
    cls T {
        !public u: U
    }

    cls Point {
        !public x: T
        !public y: T
    }

    fun f(p: &mut Point) -> std::void::Void {
        let q = p
        let x = q.x.u
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryMoveFromRefBorrowedCtx,
    test_invalid_memory_moved_from_borrowed_context,
    SppMoveFromBorrowedMemoryError, R"(
    cls T { }

    cls Point {
        !public x: T
        !public y: T
    }

    fun f(p: &Point) -> std::void::Void {
        let x = p.x
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryMoveFromRefBorrowedCtx,
    test_invalid_memory_moved_from_borrowed_context_with_symbol_alias,
    SppMoveFromBorrowedMemoryError, R"(
    cls T { }

    cls Point {
        !public x: T
        !public y: T
    }

    fun f(p: &Point) -> std::void::Void {
        let q = p
        let x = q.x
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryMoveFromRefBorrowedCtx,
    test_invalid_memory_moved_from_borrowed_context_nested,
    SppMoveFromBorrowedMemoryError, R"(
    cls U { }
    cls T {
        !public u: U
    }

    cls Point {
        !public x: T
        !public y: T
    }

    fun f(p: &Point) -> std::void::Void {
        let q = p
        let x = q.x.u
    }
)");
