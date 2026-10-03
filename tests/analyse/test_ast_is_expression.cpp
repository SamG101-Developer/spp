#include "../test_macros.hpp"

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
        case a is Point(x, y) { }
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    IsExpressionAst,
    test_valid_type_variant, R"(
    fun f() -> Void {
        let a: Str or Bool = Str::from("hello")
        case a is Str(..) {
            std::mem::ops::drop(a)
        }
        else {
            std::mem::ops::drop(a)
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
        case a is Point[S32](x, y) { }
        std::mem::ops::drop(a)
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
        let b: Bool = a is Point(x, y)
        std::mem::ops::drop(a)
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
        case a is Point(x, y) {
            let s = x
        }
        std::mem::ops::drop(a)
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
        case a is Point(x, y) and x == 1 { }
        std::mem::ops::drop(a)
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
