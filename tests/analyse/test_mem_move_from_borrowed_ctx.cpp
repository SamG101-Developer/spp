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

// Calling a "FunMov" consumes it, so calling one held through a borrow moves it out of the borrow, as a method taking
// "self" by value would (N1.6). A "FunMut" needs "&mut", a "FunRef" only "&".
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryMoveFromRefBorrowedCtx,
    test_invalid_fun_mov_called_through_an_index_borrow,
    SppMoveFromBorrowedMemoryError, R"(
    fun f(v: &mut std::vector::Vec[FunMov[(), S32]]) -> S32 {
        let r = (v[0_uz])()
        ret r
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryMoveFromRefBorrowedCtx,
    test_invalid_fun_mov_called_through_a_mutable_index_borrow,
    SppMoveFromBorrowedMemoryError, R"(
    fun f(v: &mut std::vector::Vec[FunMov[(), S32]]) -> S32 {
        let r = (v[mut 0_uz])()
        ret r
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryMoveFromRefBorrowedCtx,
    test_invalid_fun_mov_called_through_a_borrowed_parameter,
    SppMoveFromBorrowedMemoryError, R"(
    fun f(g: &FunMov[(), S32]) -> S32 {
        ret g()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryMoveFromRefBorrowedCtx,
    test_invalid_fun_mov_called_through_a_mutably_borrowed_parameter,
    SppMoveFromBorrowedMemoryError, R"(
    fun f(g: &mut FunMov[(), S32]) -> S32 {
        ret g()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryMoveFromRefBorrowedCtx,
    test_valid_fun_mut_called_through_a_mutable_index_borrow, R"(
    fun f(v: &mut std::vector::Vec[FunMut[(), S32]]) -> S32 {
        let r = (v[mut 0_uz])()
        ret r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryMoveFromRefBorrowedCtx,
    test_valid_fun_ref_called_through_an_index_borrow, R"(
    fun f(v: &std::vector::Vec[FunRef[(), S32]]) -> S32 {
        let r = (v[0_uz])()
        ret r
    }
)");

// Red: "v[i]" is a "&T" whatever "v" is, but a "&mut" argument is checked against its root symbol ("v", here mutable),
// so mutating through the index borrow is accepted - by a "&mut self" method or by calling a "FunMut". See
// docs/next-steps.md N1.11 (the lowering of "v[mut i]" itself reaches a "&mut" argument through "fwd_ref").
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryMoveFromRefBorrowedCtx,
    test_invalid_mut_self_method_called_through_an_index_borrow,
    SppInvalidMutationError, R"(
    cls IdxTok {
        !public n: S32
    }

    sup IdxTok {
        !public fun bump(&mut self) -> Void { self.n = self.n + 1 }
    }

    fun f(v: &mut std::vector::Vec[IdxTok]) -> Void {
        v[0_uz].bump()
    }
)");

// Red: as above, for a "FunMut" called through "v[i]".
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryMoveFromRefBorrowedCtx,
    test_invalid_fun_mut_called_through_an_index_borrow,
    SppInvalidMutationError, R"(
    fun f(v: &mut std::vector::Vec[FunMut[(), S32]]) -> S32 {
        let r = (v[0_uz])()
        ret r
    }
)");
