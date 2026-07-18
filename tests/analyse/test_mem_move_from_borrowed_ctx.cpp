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

    fun f(p: &mut Point) -> Void {
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

    fun f(p: &mut Point) -> Void {
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

    fun f(p: &mut Point) -> Void {
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

    fun f(p: &Point) -> Void {
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

    fun f(p: &Point) -> Void {
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

    fun f(p: &Point) -> Void {
        let q = p
        let x = q.x.u
    }
)");

// Positive counterparts: reading a COPYABLE attribute out of a borrowed context is a copy, not a
// move, so it is permitted (mem_utils.cpp: the move-from-borrowed check is skipped for partial copies).
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryMoveFromMutBorrowedCtx,
    test_valid_memory_copy_copyable_attribute_from_mut_borrowed_context, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f(p: &mut Point) -> Void {
        let x = p.x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryMoveFromRefBorrowedCtx,
    test_valid_memory_copy_copyable_attribute_from_ref_borrowed_context, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f(p: &Point) -> Void {
        let x = p.x
    }
)");
