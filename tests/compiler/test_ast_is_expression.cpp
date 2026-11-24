#include "test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  IsExpressionAst,
  test_invalid_incorrect_type_destructure,
  SppTypeMismatchError, R"(
  cls Point {
    !public x: S32
    !public y: S32
  }
  fun f() -> Void {
    let a: Point = Point(x=1, y=2)
    case a is Str(..) { }
  }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  IsExpressionAst,
  test_invalid_incorrect_type_variant_destructure,
  SppTypeMismatchError, R"(
  fun f() -> Void {
    let a: Str or Bool = Str::from("hello")
    case a is S32() { }
  }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  IsExpressionAst,
  test_invalid_incorrect_generic_destructure,
  SppTypeMismatchError, R"(
  cls Point[T] {
    !public x: T
    !public y: T
  }
  fun f() -> Void {
    let a: Point[S32] = Point[S32](x=1, y=2)
    case a is Point[Str](x, y) { }
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  IsExpressionAst,
  test_valid_is_expression_correct_type, R"(
  cls Point {
    !public x: S32
    !public y: S32
  }
  fun f() -> Void {
    let a: Point = Point(x=1, y=2)
    case a is Point(&x, &y) { }
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  IsExpressionAst,
  test_valid_type_variant, R"(
  fun f() -> Void {
    let a: Str or Bool = Str::from("hello")
    case a is Str(..) {
    }
    else {
    }
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  IsExpressionAst,
  test_valid_type_generic, R"(
  cls Point[T] {
    !public x: T
    !public y: T
  }
  fun f() -> Void {
    let a: Point[S32] = Point[S32](x=1, y=2)
    case a is Point[S32](&x, &y) { }
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  IsExpressionAst,
  test_valid_is_expression_infers_boolean, R"(
  cls Point {
    !public x: S32
    !public y: S32
  }
  fun f() -> Void {
    let a: Point = Point(x=1, y=2)
    let b: Bool = a is Point(&x, &y)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  IsExpressionAst,
  test_valid_destructure_bindings_usable, R"(
  cls Point {
    !public x: S32
    !public y: S32
  }
  fun f() -> Void {
    let a: Point = Point(x=1, y=2)
    case a is Point(&x, &y) {
      let s = x
    }
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  IsExpressionAst,
  test_valid_flow_typing_narrows_lhs, R"(
  fun f() -> Void {
    let a: StrView or Bool = false
    case a is Bool(..) {
      let b: Bool = a
    }
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  IsExpressionAst,
  test_valid_is_expression_with_guard, R"(
  cls Point {
    !public x: S32
    !public y: S32
  }
  fun f() -> Void {
    let a: Point = Point(x=1, y=2)
    case a is Point(&x, &y) and x == 1 { }
  }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  IsExpressionAst,
  test_invalid_unknown_type_in_pattern,
  SppIdentifierUnknownError, R"(
  cls Point {
    !public x: S32
    !public y: S32
  }
  fun f() -> Void {
    let a: Point = Point(x=1, y=2)
    case a is Unknown(x, y) { }
  }
)");

// An "is" binding only exists when the pattern matched, but it stays in scope after the expression.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  IsExpressionAst,
  test_invalid_is_binding_used_after_the_expression,
  SppIdentifierUnknownError, R"(
  fun f(o: Opt[S32]) -> S32 {
    let b = o is Some[S32](val)
    ret val
  }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  IsExpressionAst,
  test_invalid_is_binding_used_on_the_right_of_or,
  SppIdentifierUnknownError, R"(
  fun f(o: Opt[S32]) -> Bool {
    ret o is Some[S32](val) or val == 1
  }
)");

// The short form's else branch runs when the pattern did not match, so its bindings were never made there.
// FIXED
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  IsExpressionAst,
  test_invalid_is_binding_used_in_the_else_branch,
  SppIdentifierUnknownError, R"(
  fun f(o: Opt[S32]) -> S32 {
    ret case o is Some[S32](val) { 0 } else { val }
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  IsExpressionAst,
  test_valid_is_binding_used_in_the_matched_branch_and_the_and_chain, R"(
  fun f(o: Opt[S32]) -> S32 {
    ret case o is Some[S32](val) and val > 1 { val } else { 0 }
  }
)");

// An "is" binding is a view of the part of its subject it names: the subject keeps owning it, and after the statement
// the names it shadowed (a parameter, the subject itself) are put back.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  IsExpressionAst,
  test_valid_is_binding_names_are_put_back_after_the_statement, R"(
  fun look(o: &Opt[Str]) -> Void { }

  fun f(o: Opt[Str], s: Str) -> Void {
    let b = o is Some(val as s)
    look(&o)
    let t = s
  }
)");

// Moving a binding moves that part of the subject, on the path where the binding was matched.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  IsExpressionAst,
  test_invalid_use_of_the_subject_after_moving_its_binding,
  SppInconsistentlyInitializedMemoryUseError, R"(
  fun take(x: Str) -> Bool { ret true }

  fun look(o: &Opt[Str]) -> Void { }

  fun f(o: Opt[Str]) -> Void {
    let b = o is Some(val as y) and take(y)
    look(&o)
  }
)");

// Through a struct pattern, only the field bound is moved: the rest stays the subject's.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  IsExpressionAst,
  test_valid_moving_a_binding_moves_only_its_field, R"(
  cls IsPair {
    !public a: Str
    !public b: Str
  }

  fun take(x: Str) -> Bool { ret true }

  fun f(p: IsPair) -> Void {
    let c = p is IsPair(a as y, ..) and take(y)
    let b = p.b
  }
)");

// A chain of "and"s nests to the right, so an "is" binding of a value made on the right of one "and" is read by the
// operands after it while that value is still held.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  IsExpressionAst,
  test_valid_is_binding_of_a_temporary_read_further_along_an_and_chain, R"(
  fun mk() -> Opt[Str] { ret Some(val=Str::from("x")) }

  fun f(flag: Bool) -> Void {
    let b = flag and mk() is Some(val as y) and y.len() > 0_uz
  }
)");

// The value an "is" on the right of a loop condition's "and" makes is held exactly where the "and" is true, so the body
// reads the binding; it is dropped where the right operand answers false, and at each iteration's end.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  IsExpressionAst,
  test_valid_is_binding_of_a_temporary_on_the_right_of_and_read_in_a_loop_body, R"(
  fun mk() -> Opt[Str] { ret Some(val=Str::from("x")) }

  fun f() -> Void {
    let mut i = 0_uz
    loop i < 3_uz and mk() is Some(val as y) {
      i = i + y.len()
    }
  }
)");

// The same for a boolean "case": held in its first branch only.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  IsExpressionAst,
  test_valid_is_binding_of_a_temporary_on_the_right_of_and_read_in_a_case_branch, R"(
  fun mk() -> Opt[Str] { ret Some(val=Str::from("x")) }

  fun f(flag: Bool) -> USize {
    case flag and mk() is Some(val as y) {
      ret y.len()
    }
    else {
      ret 0_uz
    }
  }
)");
