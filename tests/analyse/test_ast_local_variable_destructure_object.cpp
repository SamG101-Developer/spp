#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureObjectAst,
    test_invalid_multiple_multi_skip,
    SppMultipleSkipMultiArgumentsError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }
    fun f() -> std::void::Void {
        let Point(x, .., ..) = Point(x=1, y=2)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureObjectAst,
    test_invalid_array_missing_attribute,
    SppArgumentMissingError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }
    fun f() -> std::void::Void {
        let Point(x) = Point(x=1, y=2)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureObjectAst,
    test_invalid_array_invalid_attribute,
    SppIdentifierUnknownError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }
    fun f() -> std::void::Void {
        let Point(x, y, z) = Point(x=1, y=2)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureObjectAst,
    test_valid_simple, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }
    fun f() -> std::void::Void {
        let Point(x, mut y) = Point(x=1, y=2)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureObjectAst,
    test_valid_with_multi_skip, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
        z: std::number::S32
    }
    fun f() -> std::void::Void {
        let Point(x, .., mut z) = Point(x=1, y=2, z=3)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureObjectAst,
    test_invalid_with_bound_multi_skip,
    SppVariableObjectDestructureWithBoundMultiSkipError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
        z: std::number::S32
    }
    fun f() -> std::void::Void {
        let Point(x, ..y, z) = Point(x=1, y=2, z=3)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureObjectAst,
    test_valid_nested_array, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
        z: std::number::S32
        dir: [std::number::S32; 3_uz]
    }
    fun f() -> std::void::Void {
        let Point(.., dir=[xd, yd, mut zd]) = Point(x=1, y=2, z=3, dir=[1, 2, 3])
        zd = 4
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureObjectAst,
    test_valid_nested_tuple, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
        z: std::number::S32
        dir: (std::number::S32, std::number::S32, std::number::S32)
    }
    fun f() -> std::void::Void {
        let Point(.., dir=(xd, yd, mut zd)) = Point(x=1, y=2, z=3, dir=(1, 2, 3))
        zd = 4
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureObjectAst,
    test_valid_nested_object, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }
    cls Line {
        start: Point
        end: Point
    }

    fun f() -> std::void::Void {
        let line = Line(start=Point(x=1, y=2), end=Point(x=3, y=4))
        let Line(start=Point(mut x as sx, mut y as sy), end=Point(mut x as ex, mut y as ey)) = line
        sx = 10
        sy = 20
        ex = 30
        ey = 40
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureObjectAst,
    test_valid_assign_to_uninitialized_parts, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }
    fun f() -> std::void::Void {
        let Point(mut x, mut y): Point
        x = 1
        y = 2
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureObjectAst,
    test_invalid_explicit_type_missing_attribute,
    SppArgumentMissingError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
        z: std::number::S32
    }
    fun f() -> std::void::Void {
        let Point(x): Point
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureObjectAst,
    test_invalid_explicit_type_too_small,
    SppIdentifierUnknownError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }
    fun f() -> std::void::Void {
        let Point(x, y, z): Point
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureObjectAst,
    test_invalid_assign_to_individual_part_wrong_type,
    SppTypeMismatchError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
        z: std::number::S32
    }
    fun f() -> std::void::Void {
        let Point(mut x, y, z): Point
        x = true
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureObjectAst,
    test_invalid_target_with_convention_ref,
    SppTypeMismatchError, R"(
    cls Point {
            x: std::number::S32
            y: std::number::S32
            z: std::number::S32
    }

    fun f(p: &Point) -> std::void::Void {
        let Point(mut x, y, z) = p
        x = 10
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureObjectAst,
    test_invalid_target_with_convention_mut,
    SppTypeMismatchError, R"(
    cls Point {
            x: std::number::S32
            y: std::number::S32
            z: std::number::S32
    }

    fun f(p: &mut Point) -> std::void::Void {
        let Point(mut x, y, z) = p
        x = 10
    }
)")

