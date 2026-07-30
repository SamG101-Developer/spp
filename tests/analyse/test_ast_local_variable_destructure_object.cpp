#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  LocalVariableDestructureObjectAst,
  test_invalid_multiple_multi_skip,
  SppMultipleRestPatternsError, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let Point(x, .., ..) = Point(x=1, y=2)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  LocalVariableDestructureObjectAst,
  test_invalid_array_missing_attribute,
  SppArgumentMissingError, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let Point(x) = Point(x=1, y=2)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  LocalVariableDestructureObjectAst,
  test_invalid_array_invalid_attribute,
  SppIdentifierUnknownError, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let Point(x, y, z) = Point(x=1, y=2)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LocalVariableDestructureObjectAst,
  test_valid_simple, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let Point(x, mut y) = Point(x=1, y=2)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LocalVariableDestructureObjectAst,
  test_valid_with_multi_skip, R"(
    cls Point {
        !public x: S32
        !public y: S32
        !public z: S32
    }

    fun f() -> Void {
        let Point(x, .., mut z) = Point(x=1, y=2, z=3)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  LocalVariableDestructureObjectAst,
  test_invalid_with_bound_multi_skip,
  SppVariableObjectDestructureWithBoundRestPatternError, R"(
    cls Point {
        !public x: S32
        !public y: S32
        !public z: S32
    }

    fun f() -> Void {
        let Point(x, ..y, z) = Point(x=1, y=2, z=3)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LocalVariableDestructureObjectAst,
  test_valid_nested_array, R"(
    cls Point {
        !public x: S32
        !public y: S32
        !public z: S32
        !public dir: [S32; 3_uz]
    }

    fun f() -> Void {
        let Point(.., dir=[xd, yd, mut zd]) = Point(x=1, y=2, z=3, dir=[1, 2, 3])
        zd = 4
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LocalVariableDestructureObjectAst,
  test_valid_nested_tuple, R"(
    cls Point {
        !public x: S32
        !public y: S32
        !public z: S32
        !public dir: (S32, S32, S32)
    }

    fun f() -> Void {
        let Point(.., dir=(xd, yd, mut zd)) = Point(x=1, y=2, z=3, dir=(1, 2, 3))
        zd = 4
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LocalVariableDestructureObjectAst,
  test_valid_nested_object, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    cls Line {
        !public start: Point
        !public end: Point
    }

    fun f() -> Void {
        let line = Line(start=Point(x=1, y=2), end=Point(x=3, y=4))
        let Line(start=Point(mut x as sx, mut y as sy), end=Point(mut x as ex, mut y as ey)) = line
        sx = 10
        sy = 20
        ex = 30
        ey = 40
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LocalVariableDestructureObjectAst,
  test_valid_assign_to_uninitialized_parts, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let Point(mut x, mut y): Point
        x = 1
        y = 2
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  LocalVariableDestructureObjectAst,
  test_invalid_explicit_type_missing_attribute,
  SppArgumentMissingError, R"(
    cls Point {
        !public x: S32
        !public y: S32
        !public z: S32
    }

    fun f() -> Void {
        let Point(x): Point
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  LocalVariableDestructureObjectAst,
  test_invalid_explicit_type_too_small,
  SppIdentifierUnknownError, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let Point(x, y, z): Point
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  LocalVariableDestructureObjectAst,
  test_invalid_assign_to_individual_part_wrong_type,
  SppTypeMismatchError, R"(
    cls Point {
        !public x: S32
        !public y: S32
        !public z: S32
    }

    fun f() -> Void {
        let Point(mut x, y, z): Point
        x = true
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  LocalVariableDestructureObjectAst,
  test_invalid_target_with_convention_ref,
  SppTypeMismatchError, R"(
    cls Point {
        !public x: S32
        !public y: S32
        !public z: S32
    }

    fun f(p: &Point) -> Void {
        let Point(mut x, y, z) = p
        x = 10
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  LocalVariableDestructureObjectAst,
  test_invalid_target_with_convention_mut,
  SppTypeMismatchError, R"(
    cls Point {
        !public x: S32
        !public y: S32
        !public z: S32
    }

    fun f(p: &mut Point) -> Void {
        let Point(mut x, y, z) = p
        x = 10
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LocalVariableDestructureObjectAst_MaterializeRhs,
  test_valid_scoped_value_case, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f(a: S32) -> Void {
        let Point(x, y) = case a == 1 { Point(x=0, y=9) } else { Point(x=1, y=8) }
        let done = false
        loop done { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LocalVariableDestructureObjectAst_MaterializeRhs,
  test_valid_scoped_value_case_chain, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f(a: S32) -> Void {
        let Point(x, y) = case a == 1 { Point(x=0, y=9) }
        else case a == 2 { Point(x=1, y=8) }
        else case a == 3 { Point(x=2, y=7) }
        else { Point(x=3, y=6) }
        let done = false
        loop done { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LocalVariableDestructureObjectAst_MaterializeRhs,
  test_valid_scoped_value_inner_scope, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let Point(x, y) = {
            let a = 1
            Point(x=a, y=9)
        }
        let done = false
        loop done { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LocalVariableDestructureObjectAst_MaterializeRhs,
  test_valid_scoped_value_moved_once, R"(
    cls Pair {
        !public a: Str
        !public b: Str
    }

    fun f(c: Bool) -> Void {
        let s = Str::from("a")
        let Pair(a, b) = case c { Pair(a=s, b=Str::from("b")) } else { Pair(a=s, b=Str::from("d")) }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  LocalVariableDestructureObjectAst_MaterializeRhs,
  test_invalid_scoped_value_already_moved,
  SppUninitializedMemoryUseError, R"(
    cls Pair {
        !public a: Str
        !public b: Str
    }

    fun f(c: Bool) -> Void {
        let s = Str::from("a")
        let t = s
        let Pair(a, b) = case c { Pair(a=s, b=Str::from("b")) } else { Pair(a=s, b=Str::from("d")) }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LocalVariableDestructureObjectAst_MaterializeRhs,
  test_valid_function_call_value, R"(
    cls Pair {
        !public a: Str
        !public b: Str
    }

    fun g() -> Pair {
        ret Pair(a=Str::from("a"), b=Str::from("b"))
    }

    fun f() -> Void {
        let Pair(a, b) = g()
        let done = false
        loop done { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  LocalVariableDestructureObjectAst_MaterializeRhs,
  test_valid_place_value_keeps_partial_moves, R"(
    cls Pair {
        !public a: Str
        !public b: Str
    }

    fun f() -> Void {
        let p = Pair(a=Str::from("a"), b=Str::from("b"))
        let Pair(a, ..) = p
        let b = p.b
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  LocalVariableDestructureObjectAst_MaterializeRhs,
  test_invalid_place_value_used_after_partial_move,
  SppPartiallyInitializedMemoryUseError, R"(
    cls Pair {
        !public a: Str
        !public b: Str
    }

    fun f() -> Void {
        let p = Pair(a=Str::from("a"), b=Str::from("b"))
        let Pair(a, ..) = p
        let q = p
    }
)");
