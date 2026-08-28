#include "../test_macros.hpp"

// The "self" parameter keeps its convention on the ast rather than on its type, so it took a different path to every
// other parameter and was never registered as a borrow at all - a value could be moved straight out of "&self" and
// nothing complained. test_mem_move_from_borrowed_ctx.cpp covers the "p: &mut Point" shape, which always worked; the
// gap was that nothing covered the "self" shape.
//
// Todo: Red until the standard library is migrated to linear ownership - see test_lin_scope_exit.cpp.

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryBorrowedSelf,
    test_invalid_move_attribute_out_of_ref_self,
    SppMoveFromBorrowedMemoryError, R"(
    cls T { }
    cls Holder { !public v: T }

    sup Holder {
        fun leak(&self) -> T {
            ret self.v
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryBorrowedSelf,
    test_invalid_move_attribute_out_of_mut_self,
    SppMoveFromBorrowedMemoryError, R"(
    cls T { }
    cls Holder { !public v: T }

    sup Holder {
        fun leak(&mut self) -> T {
            ret self.v
        }
    }
)");

// The shape the "BigUInt::clone" bug had: a "clone" that moved the field it was supposed to copy.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryBorrowedSelf,
    test_invalid_clone_moves_attribute_out_of_self,
    SppMoveFromBorrowedMemoryError, R"(
    cls T { }
    cls Holder { !public v: T }

    sup Holder ext std::clone::Clone {
        fun clone(&self) -> Self {
            ret Holder(v=self.v)
        }
    }
)");

// Taking an attribute off an owned "self" is a partial move, which is fine.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryBorrowedSelf,
    test_valid_move_attribute_out_of_owned_self, R"(
    cls T { }
    cls Holder { !public v: T }

    sup Holder {
        fun take(self) -> T {
            ret self.v
        }
    }
)");

// A case pattern is not a test - it lowers to "let v = <subject>.v" - so binding by value out of a borrowed subject
// moves out of that borrow.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryBorrowedSelf,
    test_invalid_case_pattern_binds_by_value_from_borrowed_self,
    SppMoveFromBorrowedMemoryError, R"(
    cls T { }
    cls A { !public v: T }
    cls B { }
    type AB = A or B

    fun f(x: &AB) -> Void {
        case x of {
            is A(v) { }
            else { }
        }
    }
)");

// ...and the borrow form of the same pattern reads it without taking it. This is what makes a payload inspectable
// through a borrow at all; before the fix it recorded the same move the by-value form did, so it was useless.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryBorrowedSelf,
    test_valid_case_pattern_binds_by_borrow_from_borrowed_self, R"(
    cls T { }
    cls A { !public v: T }
    cls B { }
    type AB = A or B

    fun f(x: &AB) -> Void {
        case x of {
            is A(&v) { }
            else { }
        }
    }
)");
