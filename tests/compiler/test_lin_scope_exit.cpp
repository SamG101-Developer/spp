#include "test_macros.hpp"

// Todo: Every case in this file is red until the standard library is migrated to linear ownership. The test project
//  compiles std, and std still leaves values unconsumed, so a SHOULD_PASS case fails on std rather than on its own
//  code and a SHOULD_FAIL case can throw the right error type for the wrong reason. They go green as std lands.

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearScopeExit,
    test_valid_value_moved_into_consumer, R"(
    cls Handle { !public fd: S32 }

    fun close(h: Handle) -> Void {
        let Handle(fd) = h
    }

    fun f() -> Void {
        let h = Handle(fd=1)
        close(h)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearScopeExit,
    test_valid_copyable_local_left_live, R"(
    fun f() -> Void {
        let x = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearScopeExit,
    test_valid_borrow_parameter_not_consumed, R"(
    cls Handle { !public fd: S32 }

    fun peek(h: &Handle) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearScopeExit,
    test_invalid_local_left_live,
    SppLinearValueNotConsumedError, R"(
    cls Handle { !public fd: S32 }

    fun f() -> Void {
        let h = Handle(fd=1)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearScopeExit,
    test_invalid_local_left_live_in_nested_block,
    SppLinearValueNotConsumedError, R"(
    cls Handle { !public fd: S32 }

    fun close(h: Handle) -> Void {
        let Handle(fd) = h
    }

    fun f() -> Void {
        {
            let h = Handle(fd=1)
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearScopeExit,
    test_invalid_local_left_live_in_loop_body,
    SppLinearValueNotConsumedError, R"(
    cls Handle { !public fd: S32 }

    fun f() -> Void {
        let mut i = 0
        loop i < 3 {
            let h = Handle(fd=1)
            i += 1
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearScopeExit,
    test_invalid_only_some_attributes_moved_off,
    SppLinearValueNotConsumedError, R"(
    cls Handle { !public fd: S32 }
    cls Pair {
        !public a: Handle
        !public b: Handle
    }

    fun close(h: Handle) -> Void {
        let Handle(fd) = h
    }

    fun f() -> Void {
        let p = Pair(a=Handle(fd=1), b=Handle(fd=2))
        let a = p.a
        close(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearScopeExit,
    test_valid_all_attributes_moved_off, R"(
    cls Handle { !public fd: S32 }
    cls Pair {
        !public a: Handle
        !public b: Handle
    }

    fun close(h: Handle) -> Void {
        let Handle(fd) = h
    }

    fun f() -> Void {
        let p = Pair(a=Handle(fd=1), b=Handle(fd=2))
        let a = p.a
        let b = p.b
        close(a)
        close(b)
    }
)");

// Assigning over a live non-Copy value discards the old one without consuming it. There is no implicit drop, so it
// is an error: the old value (or the part written) must be consumed first ("linear_utils::CheckOverwrite",
// "CheckOverwritePart").
// FIXED
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearScopeExit,
    test_invalid_assign_over_live_local,
    SppLinearValueNotConsumedError, R"(
    cls OwHandleA { }

    fun eat(l: OwHandleA) -> Void { let OwHandleA() = l }

    fun f() -> Void {
        let mut l = OwHandleA()
        l = OwHandleA()
        eat(l)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearScopeExit,
    test_invalid_assign_over_live_attribute,
    SppLinearValueNotConsumedError, R"(
    cls OwHandleB { }
    cls OwHolderB { !public a: OwHandleB }

    fun eat(l: OwHandleB) -> Void { let OwHandleB() = l }

    fun f() -> Void {
        let mut p = OwHolderB(a=OwHandleB())
        p.a = OwHandleB()
        let OwHolderB(a) = p
        eat(a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearScopeExit,
    test_invalid_assign_over_live_parameter,
    SppLinearValueNotConsumedError, R"(
    cls OwHandleC { }

    fun eat(l: OwHandleC) -> Void { let OwHandleC() = l }

    fun f(mut l: OwHandleC) -> Void {
        l = OwHandleC()
        eat(l)
    }
)");

// FIXED
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearScopeExit,
    test_invalid_shadowing_a_live_local,
    SppLinearValueNotConsumedError, R"(
    cls OwHandleD { }

    fun eat(l: OwHandleD) -> Void { let OwHandleD() = l }

    fun f() -> Void {
        let x = OwHandleD()
        let x = OwHandleD()
        eat(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearScopeExit,
    test_valid_assign_over_moved_local, R"(
    cls OwHandleE { }

    fun id(l: OwHandleE) -> OwHandleE { ret l }

    fun eat(l: OwHandleE) -> Void { let OwHandleE() = l }

    fun f() -> Void {
        let mut l = OwHandleE()
        l = id(l)
        eat(l)
    }
)");

// Writing over an owned value through a borrow discards it too, and through a borrow it cannot be taken first - moving
// out of a borrow is refused - so it has to be exchanged, which hands the old value back to be consumed.

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearScopeExit,
    test_invalid_assign_over_a_field_through_a_mut_borrow,
    SppLinearValueNotConsumedError, R"(
    cls OwHolderF { !public name: Str }

    sup OwHolderF {
        !public fun reset(&mut self) -> Void {
            self.name = Str::from("x")
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearScopeExit,
    test_invalid_assign_through_a_mut_borrow_deref,
    SppLinearValueNotConsumedError, R"(
    fun reset(s: &mut Str) -> Void {
        s@ = Str::from("x")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearScopeExit,
    test_valid_exchange_a_field_through_a_mut_borrow, R"(
    cls OwHolderG { !public name: Str }

    sup OwHolderG {
        !public fun reset(&mut self) -> Void {
            let old = std::mem::ops::replace(&mut self.name, Str::from("x"))
            std::mem::ops::drop(old)
        }
    }
)");

// A holder of loans owns its environment or frame, so it is owed like any other value. It cannot be moved away, but a
// drop ends it and its loans together.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearScopeExit,
    test_invalid_mut_capturing_closure_never_dropped,
    SppLinearValueNotConsumedError, R"(
    fun f() -> Void {
        let mut x = 1_s32
        let mut c = (caps &mut x) { x@ = 2_s32 }
        c()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearScopeExit,
    test_valid_mut_capturing_closure_dropped_then_capture_used, R"(
    fun f() -> S32 {
        let mut x = 1_s32
        let mut c = (caps &mut x) { x@ = 2_s32 }
        c()
        std::mem::ops::drop(c)
        ret x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearScopeExit,
    test_invalid_borrowing_generator_never_dropped,
    SppLinearValueNotConsumedError, R"(
    cor g(a: &mut S32) -> Gen[&mut S32] { gen a }

    fun f() -> Void {
        let mut x = 1_s32
        let mut it = g(&mut x)
        let y = it.res()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearScopeExit,
    test_valid_borrowing_generator_dropped, R"(
    cor g(a: &mut S32) -> Gen[&mut S32] { gen a }

    fun f() -> S32 {
        let mut x = 1_s32
        let mut it = g(&mut x)
        let y = it.res()
        std::mem::ops::drop(it)
        ret x
    }
)");

// Only a drop ends a holder: given to anything else it would carry its borrows on.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearScopeExit,
    test_invalid_holder_moved_into_a_function_other_than_drop,
    SppMovingEscapingBorrowedMemoryError, R"(
    fun keep[T](t: T) -> T { ret t }

    fun f() -> Void {
        let mut x = 1_s32
        let mut c = (caps &mut x) { x@ = 2_s32 }
        let d = keep(c)
    }
)");

// An inherited default method is checked per class: "Clone::clone_from" writes over "self@", which drops the old value
// of a class that owns anything. The template, over an abstract "Self", says nothing until a class is chosen.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestLinearScopeExit,
    test_invalid_inherited_clone_from_drops_an_owning_class,
    SppLinearValueNotConsumedError, R"(
    cls Named {
        !public s: Str
    }

    sup Named ext std::clone::Clone {
        fun clone(&self) -> Named { ret Named(s=self.s.clone()) }
    }

    fun f() -> Void {
        let a = Named(s=Str::from("a"))
        let mut b = Named(s=Str::from("b"))
        b.clone_from(&a)
        std::mem::ops::drop(a)
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearScopeExit,
    test_valid_clone_from_overridden_to_exchange, R"(
    cls Named {
        !public s: Str
    }

    sup Named ext std::clone::Clone {
        fun clone(&self) -> Named { ret Named(s=self.s.clone()) }
        fun clone_from(&mut self, source: &Named) -> Void {
            let old = std::mem::ops::replace(self, source.clone())
            std::mem::ops::drop(old)
        }
    }

    fun f() -> Void {
        let a = Named(s=Str::from("a"))
        let mut b = Named(s=Str::from("b"))
        b.clone_from(&a)
        std::mem::ops::drop(a)
        std::mem::ops::drop(b)
    }
)");

// A "Copy" class drops nothing by writing over itself. Its copy of the method also reads "source: &Self" as the class:
// only "self" was pinned to the receiver once, so "source.clone()" was the abstract "Self" (E1).
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLinearScopeExit,
    test_valid_inherited_clone_from_on_a_copy_class, R"(
    cls Pt {
        !public x: S32
    }

    sup Pt ext std::copy::Copy { }

    sup Pt ext std::clone::Clone {
        fun clone(&self) -> Pt { ret Pt(x=self.x) }
    }

    fun f() -> S32 {
        let a = Pt(x=1)
        let mut b = Pt(x=2)
        b.clone_from(&a)
        ret b.x
    }
)");
