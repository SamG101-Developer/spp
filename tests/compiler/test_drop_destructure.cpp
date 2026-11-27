#include "test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDestructure,
  test_valid_destructure_consumes_whole_value, R"(
  cls Handle { !public fd: S32 }

  fun f() -> Void {
    let h = Handle(fd=1)
    let Handle(fd) = h
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDestructure,
  test_valid_tuple_destructure_consumes, R"(
  cls Handle { !public fd: S32 }

  fun f() -> Void {
    let t = (Handle(fd=1), Handle(fd=2))
    let (a, b) = t
    let Handle(fd as fd_a) = a
    let Handle(fd as fd_b) = b
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDestructure,
  test_valid_fieldless_value_destructured, R"(
  cls Marker { }

  fun f() -> Void {
    let m = Marker()
    let Marker() = m
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDestructure,
  test_valid_fieldless_value_left_live, R"(
  cls Marker { }

  fun f() -> Void {
    let m = Marker()
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDestructure,
  test_valid_copy_attribute_read_does_not_consume, R"(
  cls Handle { !public fd: S32 }

  fun f() -> Void {
    let h = Handle(fd=1)
    let fd = h.fd
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDestructure,
  test_valid_reading_through_a_borrow_does_not_consume, R"(
  cls Handle { !public fd: S32 }

  fun peek(h: &Handle) -> S32 {
    ret h.fd
  }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestDropDestructure,
  test_invalid_destructure_through_a_borrow,
  SppTypeMismatchError, R"(
  cls Handle { !public fd: S32 }

  fun peek(h: &Handle) -> S32 {
    let Handle(fd) = h
    ret fd
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDestructure,
  test_valid_destructure_skips_an_owned_attribute, R"(
  cls Point {
    !public x: Str
    !public y: Str
  }

  fun f(p: Point) -> Void {
    let Point(x, ..) = p
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDestructure,
  test_valid_tuple_destructure_skips_an_owned_element, R"(
  fun f(t: (Str, Str)) -> Void {
    let (a, ..) = t
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDestructure,
  test_valid_case_branch_skips_an_owned_attribute, R"(
  cls Point {
    !public x: Str
    !public y: Str
  }

  fun f(p: Point) -> Void {
    case p of {
      is Point(x, ..) { }
    }
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDestructure,
  test_valid_destructure_skips_a_copyable_attribute, R"(
  cls Point {
    !public x: Str
    !public y: U32
  }

  fun f(p: Point) -> Void {
    let Point(x, ..) = p
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDestructure,
  test_valid_shape_test_takes_nothing_apart, R"(
  cls Point {
    !public x: Str
    !public y: Str
  }

  fun f(p: Point) -> Void {
    let b = p is Point(..)
  }
)");

// A destructure binding nothing reads no part, and the parts are all the old check looked at, so a value that had
// already been moved was consumed again.
// FIXED
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestDropDestructure,
  test_invalid_empty_destructure_of_a_moved_value,
  SppUninitializedMemoryUseError, R"(
  cls DsEmpty { }

  fun eat(l: DsEmpty) -> Void { let DsEmpty() = l }

  fun f() -> Void {
    let l = DsEmpty()
    eat(l)
    let DsEmpty() = l
  }
)");

// A value made on the spot is held by a hidden temporary, which keeps the skipped parts as a named value does, and
// drops them at the end of the statement.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDestructure,
  test_valid_destructure_of_a_temporary_skips_an_owned_attribute, R"(
  cls Point {
    !public x: Str
    !public y: Str
  }

  fun mk() -> Point { ret Point(x=Str::from("x"), y=Str::from("y")) }

  fun f() -> Void {
    let Point(x, ..) = mk()
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDestructure,
  test_valid_tuple_destructure_of_a_temporary_skips_an_owned_element, R"(
  fun mk() -> (Str, Str) { ret (Str::from("x"), Str::from("y")) }

  fun f() -> Void {
    let (x, ..) = mk()
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDestructure,
  test_valid_destructure_of_a_temporary_skips_only_copy_parts, R"(
  cls Point {
    !public x: Str
    !public y: S32
  }

  fun mk() -> Point { ret Point(x=Str::from("x"), y=1) }

  fun f() -> Void {
    let Point(x, ..) = mk()
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDestructure,
  test_valid_ret_before_tuple_destructure_of_a_temporary, R"(
  fun mk() -> (Str, Str) { ret (Str::from("x"), Str::from("y")) }

  fun f(b: Bool) -> Void {
    case b { ret }
    let (x, y) = mk()
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDestructure,
  test_valid_ret_before_object_destructure_of_a_temporary, R"(
  cls Point {
    !public x: Str
    !public y: Str
  }

  fun mk() -> Point { ret Point(x=Str::from("x"), y=Str::from("y")) }

  fun f(b: Bool) -> Void {
    case b { ret }
    let Point(x, y) = mk()
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDestructure,
  test_valid_ret_after_tuple_destructure_of_a_temporary_leaves_a_binding_live, R"(
  fun mk() -> (Str, Str) { ret (Str::from("x"), Str::from("y")) }

  fun f(b: Bool) -> Void {
    let (x, y) = mk()
    drop(x)
    case b { ret }
  }
)");

// A destructure takes the value apart: its type's own "drop" never runs on it, so skipping an owned part is no E106.
// The skipped part is dropped field by field by the value's owner.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDestructure,
  test_valid_destructure_skipping_an_owned_part_of_a_destructible_value, R"(
  cls Held {
    !public a: Str
    !public b: Str
  }

  sup Held ext std::ops::drop::Drop {
    fun drop(self) -> Void { }
  }

  fun f() -> Void {
    let h = Held(a=Str::from("a"), b=Str::from("b"))
    let Held(a, ..) = h
  }
)");

// Binding nothing takes it apart all the same: how a "close"-style consumer disarms the type's own "drop".
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestDropDestructure,
  test_valid_destructure_binding_nothing_disarms_the_destructor, R"(
  cls Held { !public a: Str }

  sup Held ext std::ops::drop::Drop {
    fun drop(self) -> Void { }
  }

  sup Held {
    fun close(self) -> Void {
      let Self(..) = self
    }
  }
)");

// A field moved out by access is no destructure: the value's own "drop" would run on what is left (E106).
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestDropDestructure,
  test_invalid_field_moved_out_of_a_destructible_value_by_access,
  SppPartialMoveOfDestructibleValueError, R"(
  cls Held {
    !public a: Str
    !public b: Str
  }

  sup Held ext std::ops::drop::Drop {
    fun drop(self) -> Void { }
  }

  fun f() -> Void {
    let h = Held(a=Str::from("a"), b=Str::from("b"))
    let a = h.a
  }
)");
