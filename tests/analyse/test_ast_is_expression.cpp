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
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    IsExpressionAst,
    test_valid_type_variant, R"(
    fun f() -> Void {
        let a: Str or Bool = Str::from("hello")
        case a is Str(..) { }
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
