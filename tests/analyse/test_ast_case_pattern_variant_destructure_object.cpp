#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_invalid_multiple_multi_skip,
  SppMultipleRestPatternsError, R"(
    cls Point {
        x: Str
        y: Str
    }

    fun f(p: Point) -> Void {
        case p is Point(.., ..) { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_invalid_bound_multi_skip,
  SppVariableObjectDestructureWithBoundRestPatternError, R"(
    cls Point {
        x: Str
        y: Str
    }

    fun f(p: Point) -> Void {
        case p is Point(..x) { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_invalid_missing_attribute,
  SppArgumentMissingError, R"(
    cls Point {
        x: Str
        y: Str
    }

    fun f(p: Point) -> Void {
        case p is Point(x) { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_invalid_invalid_attribute,
  SppIdentifierUnknownError, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    fun f(p: Point) -> Void {
        case p is Point(x, y, z) { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_invalid_aliasing_attributes_use_old_name,
  SppIdentifierUnknownError, R"(
    cls Point1 {
        !public x: Str
        !public y: Str
    }

    fun f(p: Point1) -> Void {
        case p is Point1(x as x_value, ..) { let xxx = x }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_invalid_type_not_in_variant,
  SppTypeMismatchError, R"(
    cls Point1 {
        x: Str
    }

    cls Point2 {
        x: Str
    }

    cls Point3 {
        x: Str
    }

    fun f(p: Point1 or Point2) -> Void {
        case p of {
            is Point3(x) { }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_valid_value_only, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    fun f(p: Point) -> Void {
        case p is Point(x, y) { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_valid_value_and_unbound_multi_skip, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    fun f(p: Point) -> Void {
        # Bound by borrow: a binary "is" matches on one path only, so binding by value would leave "p" partially
        # moved on that path and intact on the other, which is not a state it can be discarded from.
        case p is Point(&x, ..) { }
        std::mem::ops::drop(p)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_valid_multiple_branches, R"(
    cls Point1 {
        !public x: Str
        !public y: Str
    }

    cls Point2 {
        !public x: Str
        !public y: Str
    }

    fun f(p: Point1 or Point2) -> Void {
        case p of {
            is Point1(x, y) { }
            is Point2(x, y) { }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_valid_valid_value_and_alias, R"(
    cls Point1 {
        !public x: Str
        !public y: Str
    }

    fun f(p: Point1) -> Void {
        case p of {
            is Point1(x as x_value, ..) {
                let xxx = x_value
                std::mem::ops::drop(xxx)
            }
            is Point1(y as y_value, ..) {
                let yyy = y_value
                std::mem::ops::drop(yyy)
            }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_valid_nested_array_in_object, R"(
    cls Point {
        !public x: std::array::Arr[Str, 2_uz]
        !public y: Str
    }
    fun f(p: Point) -> Void {
        case p is Point(x=[a, b], y) { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_valid_nested_tuple_in_object, R"(
    cls Point {
        !public x: (Str, Str)
        !public y: Str
    }
    fun f(p: Point) -> Void {
        case p is Point(x=(a, b), y) { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_valid_nested_object_in_object, R"(
    # Copyable attributes, so the aliased bindings copy rather than move: a binary "is" matches on one path only, and
    # binding by value would leave "p" partially moved there and intact on the other. What this pins is the shape of
    # the nested pattern, which the attribute type is incidental to.
    cls Point {
        !public x: S32
        !public y: S32
    }

    cls Line {
        !public start: Point
        !public end: Point
    }

    fun f(p: Line) -> Void {
        case p is Line(start=Point(x as x1, y as y1), end=Point(x as x2, y as y2)) { }
        std::mem::ops::drop(p)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_valid_variant_narrowing_opt, R"(
    fun f(p: Opt[Str]) -> Void {
        case p of {
            is Some[Str](val) { }
            is None() { }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_valid_variant_narrowing_res, R"(
    fun f(p: Res[Str, Bool]) -> Void {
        case p of {
            is Pass[Str](val) { }
            is Fail[Bool](err) { }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_valid_variant_narrowing_distinct_attributes, R"(
    cls A {
        !public a: Str
    }

    cls B {
        !public b: Str
    }

    fun f(p: A or B) -> Void {
        case p of {
            is A(a) { }
            is B(b) { }
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_valid_variant_narrowing_bind_and_use, R"(
    fun f(p: Opt[Str]) -> Str {
        ret case p of {
            is Some[Str](val) { val }
            else { Str::from("none") }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_invalid_variant_wrong_generic_argument,
  SppTypeMismatchError, R"(
    fun f(p: Opt[Str]) -> Void {
        case p of {
            is Some[Bool](val) { }
            is None() { }
        }
    }
)");

// A binding written with a convention names the attribute rather than a copy of it, so its slot holds an address.
// Stage 7 gave the symbol the borrow type all along, but codegen sized the slot from the initializer - so "x" held
// the attribute's value, and writing through it stored into a number instead of into the attribute.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_valid_bind_by_mut_borrow_and_write_through_it, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }
    sup Point ext Copy { }

    fun f() -> Void {
        let p = Point(x=10, y=20)
        let a = 123
        case p of {
            is Point(&mut x, ..) { x@ = a }
            else { }
        }
    }
)");

// The immutable form of the same binding: readable through, and not writable.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  CasePatternVariantDestructureObjectAst,
  test_invalid_write_through_an_immutable_borrow_binding,
  SppInvalidMutationError, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }
    sup Point ext Copy { }

    fun f() -> Void {
        let p = Point(x=10, y=20)
        let a = 123
        case p of {
            is Point(&x, ..) { x@ = a }
            else { }
        }
    }
)");
