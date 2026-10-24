#include "test_macros.hpp"

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

// "v[i]" is a "&T" whatever "v" is, so nothing can be mutated through it - by a "&mut self" method or by calling a
// "FunMut" - even when "v" itself is mutable. "v[mut i]" is the borrow to mutate through.
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

// As above, for a "FunMut" called through "v[i]".
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryMoveFromRefBorrowedCtx,
    test_invalid_fun_mut_called_through_an_index_borrow,
    SppInvalidMutationError, R"(
    fun f(v: &mut std::vector::Vec[FunMut[(), S32]]) -> S32 {
        let r = (v[0_uz])()
        ret r
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryMoveFromRefBorrowedCtx,
    test_valid_mut_self_method_called_through_a_mut_index_borrow, R"(
    cls IdxTok {
        !public n: S32
    }

    sup IdxTok {
        !public fun bump(&mut self) -> Void { self.n = self.n + 1 }
    }

    fun f(v: &mut std::vector::Vec[IdxTok]) -> Void {
        v[mut 0_uz].bump()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryMoveFromRefBorrowedCtx,
    test_valid_fun_mut_called_through_a_mut_index_borrow, R"(
    fun f(v: &mut std::vector::Vec[FunMut[(), S32]]) -> S32 {
        let r = (v[mut 0_uz])()
        ret r
    }
)");

// A method reached through a forward ("Vec" to "View") is borrowed as it takes "self": "index_mut" mutably, "len"
// immutably, so reading through the forward beside another "&" borrow is fine.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryMoveFromRefBorrowedCtx,
    test_valid_read_through_a_forward_beside_an_immutable_borrow, R"(
    fun g(v: &std::vector::Vec[S32], n: USize) -> Void { }

    fun f(mut v: std::vector::Vec[S32]) -> Void {
        g(&v, v.len())
        std::mem::ops::drop(v)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryMoveFromRefBorrowedCtx,
    test_invalid_mut_index_through_an_immutable_binding,
    SppInvalidMutationError, R"(
    fun f(v: std::vector::Vec[S32]) -> Void {
        let x = v[mut 0_uz]
        std::mem::ops::drop(v)
    }
)");
