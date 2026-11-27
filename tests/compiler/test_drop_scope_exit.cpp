#include "test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
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
  TestDropScopeExit,
  test_valid_copyable_local_left_live, R"(
  fun f() -> Void {
    let x = 123
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_borrow_parameter_not_consumed, R"(
  cls Handle { !public fd: S32 }

  fun peek(h: &Handle) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_local_left_live, R"(
  cls Handle { !public fd: S32 }

  fun f() -> Void {
    let h = Handle(fd=1)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_local_left_live_in_nested_block, R"(
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

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_local_left_live_in_loop_body, R"(
  cls Handle { !public fd: S32 }

  fun f() -> Void {
    let mut i = 0
    loop i < 3 {
      let h = Handle(fd=1)
      i += 1
    }
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_only_some_attributes_moved_off, R"(
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
  TestDropScopeExit,
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

// Assigning over a live non-Copy value drops the old one (or the part written) before the write
// ("drop_utils::OwnedBeforeWrite").
// FIXED
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_assign_over_live_local, R"(
  cls OwHandleA { }

  fun eat(l: OwHandleA) -> Void { let OwHandleA() = l }

  fun f() -> Void {
    let mut l = OwHandleA()
    l = OwHandleA()
    eat(l)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_assign_over_live_attribute, R"(
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

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_refill_a_part_taken_on_every_branch, R"(
  cls OwHandleB { }
  cls OwHolderB { !public a: OwHandleB }

  fun eat(l: OwHandleB) -> Void { let OwHandleB() = l }

  fun f(c: Bool) -> Void {
    let mut p = OwHolderB(a=OwHandleB())
    case c { eat(p.a) }
    else { eat(p.a) }
    p.a = OwHandleB()
    let OwHolderB(a) = p
    eat(a)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_moved_whole_on_one_branch_taken_apart_on_the_other, R"(
  cls OwHandleB { }
  cls OwHolderB { !public a: OwHandleB }

  fun eat(l: OwHandleB) -> Void { let OwHandleB() = l }
  fun eat_holder(h: OwHolderB) -> Void {
    let OwHolderB(a) = h
    eat(a)
  }

  fun f(c: Bool) -> Void {
    let p = OwHolderB(a=OwHandleB())
    case c { eat(p.a) }
    else { eat_holder(p) }
  }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestDropScopeExit,
  test_invalid_refill_a_part_taken_on_only_one_branch,
  SppInconsistentlyInitializedMemoryUseError, R"(
  cls OwHandleB { }
  cls OwHolderB { !public a: OwHandleB }

  fun eat(l: OwHandleB) -> Void { let OwHandleB() = l }

  fun f(c: Bool) -> Void {
    let mut p = OwHolderB(a=OwHandleB())
    case c { eat(p.a) }
    else { }
    p.a = OwHandleB()
    let OwHolderB(a) = p
    eat(a)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_assign_over_live_parameter, R"(
  cls OwHandleC { }

  fun eat(l: OwHandleC) -> Void { let OwHandleC() = l }

  fun f(mut l: OwHandleC) -> Void {
    l = OwHandleC()
    eat(l)
  }
)");

// FIXED
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_shadowing_a_live_local, R"(
  cls OwHandleD { }

  fun eat(l: OwHandleD) -> Void { let OwHandleD() = l }

  fun f() -> Void {
    let x = OwHandleD()
    let x = OwHandleD()
    eat(x)
  }
)");

// A "let" shadowing a binding in its own scope keeps that binding's value, whatever form the shadowing takes: the old
// one can never be named again, and is dropped at its scope's end ("Scope::GetOwners" lists displaced bindings).

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_shadowing_a_local_its_own_value_consumes, R"(
  cls OwHandleS { }

  fun id(l: OwHandleS) -> OwHandleS { ret l }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun f() -> Void {
    let x = OwHandleS()
    let x = id(x)
    eat(x)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_shadowing_a_consumed_local, R"(
  cls OwHandleS { }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun f() -> Void {
    let x = OwHandleS()
    eat(x)
    let x = OwHandleS()
    eat(x)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_shadowing_a_live_local_with_an_uninitialized_let, R"(
  cls OwHandleS { }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun f() -> Void {
    let x = OwHandleS()
    let x: OwHandleS
    x = OwHandleS()
    eat(x)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_shadowing_a_live_parameter, R"(
  cls OwHandleS { }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun f(x: OwHandleS) -> Void {
    let x = OwHandleS()
    eat(x)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_shadowing_a_live_local_by_a_tuple_destructure, R"(
  cls OwHandleS { }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun f() -> Void {
    let x = OwHandleS()
    let (x, y) = (OwHandleS(), OwHandleS())
    eat(x)
    eat(y)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_shadowing_a_live_local_by_an_object_destructure, R"(
  cls OwHandleS { }
  cls OwPairS {
    !public x: OwHandleS
    !public y: OwHandleS
  }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun f() -> Void {
    let x = OwHandleS()
    let OwPairS(x, y) = OwPairS(x=OwHandleS(), y=OwHandleS())
    eat(x)
    eat(y)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_shadowing_a_partially_moved_local, R"(
  cls OwHandleS { }
  cls OwPairS {
    !public a: OwHandleS
    !public b: OwHandleS
  }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun f() -> Void {
    let p = OwPairS(a=OwHandleS(), b=OwHandleS())
    let a = p.a
    eat(a)
    let p = OwPairS(a=OwHandleS(), b=OwHandleS())
    let OwPairS(a, b) = p
    eat(a)
    eat(b)
  }
)");

// An "is" binding does not outlive its statement, so it hides a binding of the same name only within it: the block
// puts the hidden one back as the statement ends ("Scope::EndStatement"), and it is dropped at its scope's end.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_live_local_hidden_by_a_same_named_is_binding, R"(
  cls OwHandleS { }

  fun f(o: Opt[OwHandleS]) -> Void {
    let x = OwHandleS()
    let b = o is Some[OwHandleS](&val as x)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_local_named_after_a_same_named_is_binding, R"(
  cls OwHandleS { }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun f(o: Opt[OwHandleS]) -> Void {
    let x = OwHandleS()
    let b = o is Some[OwHandleS](&val as x)
    drop(o)
    eat(x)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_local_used_by_a_same_named_is_binding_before_and_after, R"(
  cls OwHandleS { }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun peek(l: &OwHandleS) -> Bool { ret true }

  fun f(o: Opt[OwHandleS]) -> Void {
    let x = OwHandleS()
    let b = o is Some[OwHandleS](&val as x) and peek(x)
    drop(o)
    eat(x)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_live_parameter_hidden_by_a_same_named_is_binding, R"(
  cls OwHandleS { }

  fun f(o: Opt[OwHandleS], x: OwHandleS) -> Void {
    let b = o is Some[OwHandleS](&val as x)
  }
)");

// An "is" binding that takes its value is dropped at the end of its statement too, once it has a name no later line can
// reach.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_moving_is_binding_over_a_live_local_not_consumed_in_its_statement, R"(
  cls OwHandleS { }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun f(o: Opt[OwHandleS]) -> Void {
    let x = OwHandleS()
    let b = o is Some[OwHandleS](val as x)
    eat(x)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_moving_is_binding_over_a_live_local_consumed_in_its_statement, R"(
  cls OwHandleS { }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun took(l: OwHandleS) -> Bool {
    eat(l)
    ret true
  }

  fun f(o: Opt[OwHandleS]) -> Void {
    let x = OwHandleS()
    let b = o is Some[OwHandleS](val as x) and took(x)
    eat(x)
  }
)");

// An alias in an object destructure is the binding, so it shadows as any binding does: in a "let", for the rest of
// the scope; in a "case" pattern, for the branch alone, the outer binding being still owned after the "case".

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_shadowing_a_live_local_by_an_aliased_let_destructure, R"(
  cls OwHandleS { }
  cls OwPairS {
    !public a: OwHandleS
    !public b: OwHandleS
  }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun f() -> Void {
    let x = OwHandleS()
    let OwPairS(a as x, b) = OwPairS(a=OwHandleS(), b=OwHandleS())
    eat(x)
    eat(b)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_shadowing_a_consumed_local_by_an_aliased_let_destructure, R"(
  cls OwHandleS { }
  cls OwPairS {
    !public a: OwHandleS
    !public b: OwHandleS
  }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun f() -> Void {
    let x = OwHandleS()
    eat(x)
    let OwPairS(a as x, b) = OwPairS(a=OwHandleS(), b=OwHandleS())
    eat(x)
    eat(b)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_aliased_case_destructure_keeps_the_outer_binding, R"(
  cls OwHandleS { }
  cls OwPairS {
    !public a: OwHandleS
    !public b: OwHandleS
  }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun f(p: OwPairS) -> Void {
    let x = OwHandleS()
    case p of {
      is OwPairS(a as x, b) {
        eat(x)
        eat(b)
      }
    }
    eat(x)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_aliased_case_destructure_leaves_the_outer_binding_live, R"(
  cls OwHandleS { }
  cls OwPairS {
    !public a: OwHandleS
    !public b: OwHandleS
  }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun f(p: OwPairS) -> Void {
    let x = OwHandleS()
    case p of {
      is OwPairS(a as x, b) {
        eat(x)
        eat(b)
      }
    }
  }
)");

// A "case" branch's bindings, its patterns' and an "is" condition's, end with it, and are dropped there if still owned.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_aliased_case_destructure_binding_not_consumed_in_its_branch, R"(
  cls OwHandleS { }
  cls OwPairS {
    !public a: OwHandleS
    !public b: OwHandleS
  }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun f(p: OwPairS) -> Void {
    let x = OwHandleS()
    case p of {
      is OwPairS(a as x, b) {
        eat(b)
      }
    }
    eat(x)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_aliased_is_condition_keeps_the_outer_binding, R"(
  cls OwHandleS { }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun f(o: Opt[OwHandleS]) -> Void {
    let x = OwHandleS()
    case o is Some[OwHandleS](val as x) {
      eat(x)
    }
    else {
    }
    eat(x)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_aliased_is_condition_binding_not_consumed_in_its_branch, R"(
  cls OwHandleS { }

  fun f(o: Opt[OwHandleS]) -> Void {
    case o is Some[OwHandleS](val as y) { }
    else {
    }
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_early_return_in_a_statement_whose_is_binding_hides_a_live_local, R"(
  cls OwHandleS { }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun g() -> Res[Bool, Bool] { ret Pass[Bool](val=true) }

  fun f(o: &Opt[OwHandleS]) -> Res[Bool, Bool] {
    let x = OwHandleS()
    let b = o is Some[OwHandleS](&val as x) and g()?
    eat(x)
    ret Pass[Bool](val=b)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_loop_condition_binding_not_consumed_in_the_body, R"(
  cls OwHandleS { }

  fun mk() -> Opt[OwHandleS] { ret None }

  fun f() -> Void {
    loop mk() is Some[OwHandleS](val as y) { }
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_loop_condition_binding_consumed_in_the_body, R"(
  cls OwHandleS { }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun mk() -> Opt[OwHandleS] { ret None }

  fun f() -> Void {
    loop mk() is Some[OwHandleS](val as y) {
      eat(y)
    }
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_loop_condition_binding_not_consumed_before_a_skip, R"(
  cls OwHandleS { }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun mk() -> Opt[OwHandleS] { ret None }

  fun f(c: Bool) -> Void {
    loop mk() is Some[OwHandleS](val as y) {
      case c { skip }
      eat(y)
    }
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_loop_condition_binding_consumed_before_a_skip, R"(
  cls OwHandleS { }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun mk() -> Opt[OwHandleS] { ret None }

  fun f(c: Bool) -> Void {
    loop mk() is Some[OwHandleS](val as y) {
      eat(y)
      case c { skip }
    }
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_loop_condition_binding_consumed_in_a_nested_loop, R"(
  cls OwHandleS { }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun mk() -> Opt[OwHandleS] { ret None }

  fun f() -> Void {
    loop mk() is Some[OwHandleS](val as x) {
      eat(x)
      loop mk() is Some[OwHandleS](val as y) {
        eat(y)
      }
    }
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_is_binding_not_consumed_in_its_statement, R"(
  cls OwHandleS { }

  fun f(o: Opt[OwHandleS]) -> Void {
    let b = o is Some[OwHandleS](val as y)
    let c = 1
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_shadowing_in_a_nested_scope_keeps_the_outer_binding, R"(
  cls OwHandleS { }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun f() -> Void {
    let x = OwHandleS()
    {
      let x = OwHandleS()
      eat(x)
    }
    eat(x)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_shadowing_in_a_nested_scope_leaves_the_outer_binding_live, R"(
  cls OwHandleS { }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun f() -> Void {
    let x = OwHandleS()
    {
      let x = OwHandleS()
      eat(x)
    }
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_shadowing_a_live_local_inside_a_loop_body, R"(
  cls OwHandleS { }

  fun eat(l: OwHandleS) -> Void { let OwHandleS() = l }

  fun f() -> Void {
    loop true {
      let x = OwHandleS()
      let x = OwHandleS()
      eat(x)
      exit
    }
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
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

// Writing over an owned value through a borrow drops the old value before the write too. Through a borrow it cannot be
// taken first - moving out of a borrow is refused - so to keep it, it is exchanged, which hands the old value back.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_assign_over_a_field_through_a_mut_borrow, R"(
  cls OwHolderF { !public name: Str }

  sup OwHolderF {
    !public fun reset(&mut self) -> Void {
      self.name = Str::from("x")
    }
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_assign_through_a_mut_borrow_deref, R"(
  fun reset(s: &mut Str) -> Void {
    s@ = Str::from("x")
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_exchange_a_field_through_a_mut_borrow, R"(
  cls OwHolderG { !public name: Str }

  sup OwHolderG {
    !public fun reset(&mut self) -> Void {
      let old = std::mem::ops::replace(&mut self.name, Str::from("x"))
    }
  }
)");

// A holder of loans owns its environment or frame, so it is dropped at its scope's end like any other value. It cannot
// be moved away, but a drop ends it and its loans together.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_mut_capturing_closure_never_dropped, R"(
  fun f() -> Void {
    let mut x = 1_s32
    let mut c = |caps &mut x| { x@ = 2_s32 }
    c()
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_mut_capturing_closure_dropped_then_capture_used, R"(
  fun f() -> S32 {
    let mut x = 1_s32
    let mut c = |caps &mut x| { x@ = 2_s32 }
    c()
    ret x
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_borrowing_generator_never_dropped, R"(
  cor g(a: &mut S32) -> Gen[&mut S32] { gen a }

  fun f() -> Void {
    let mut x = 1_s32
    let mut it = g(&mut x)
    let y = it.res()
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_borrowing_generator_dropped, R"(
  cor g(a: &mut S32) -> Gen[&mut S32] { gen a }

  fun f() -> S32 {
    let mut x = 1_s32
    let mut it = g(&mut x)
    let y = it.res()
    ret x
  }
)");

// Only a drop ends a holder: given to anything else it would carry its borrows on.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestDropScopeExit,
  test_invalid_holder_moved_into_a_function_other_than_drop,
  SppMovingEscapingBorrowedMemoryError, R"(
  fun keep[T](t: T) -> T { ret t }

  fun f() -> Void {
    let mut x = 1_s32
    let mut c = |caps &mut x| { x@ = 2_s32 }
    let d = keep(c)
  }
)");

// An inherited default method is checked per class: "Clone::clone_from" writes over "self@", which drops the old value
// of a class that owns anything. The template, over an abstract "Self", says nothing until a class is chosen.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_inherited_clone_from_drops_an_owning_class, R"(
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
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
  test_valid_clone_from_overridden_to_exchange, R"(
  cls Named {
    !public s: Str
  }

  sup Named ext std::clone::Clone {
    fun clone(&self) -> Named { ret Named(s=self.s.clone()) }
    fun clone_from(&mut self, source: &Named) -> Void {
      let old = std::mem::ops::replace(self, source.clone())
    }
  }

  fun f() -> Void {
    let a = Named(s=Str::from("a"))
    let mut b = Named(s=Str::from("b"))
    b.clone_from(&a)
  }
)");

// A "Copy" class drops nothing by writing over itself. Its copy of the method also reads "source: &Self" as the class:
// only "self" was pinned to the receiver once, so "source.clone()" was the abstract "Self" (E1).
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropScopeExit,
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
