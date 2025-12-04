#include "../test_macros.hpp"
import testex;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CasePatternVariantDestructureObjectAst,
    test_invalid_multiple_multi_skip,
    SppMultipleSkipMultiArgumentsError, R"(
    cls Point {
        x: std::bignum::bigint::BigInt
        y: std::bignum::bigint::BigInt
    }

    fun f(p: Point) -> std::void::Void {
        case p is Point(.., ..) { }
    }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CasePatternVariantDestructureObjectAst,
    test_invalid_bound_multi_skip,
    SppVariableObjectDestructureWithBoundMultiSkipError, R"(
    cls Point {
        x: std::bignum::bigint::BigInt
        y: std::bignum::bigint::BigInt
    }

    fun f(p: Point) -> std::void::Void {
        case p is Point(..x) { }
    }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CasePatternVariantDestructureObjectAst,
    test_invalid_missing_attribute,
    SppArgumentMissingError, R"(
    cls Point {
        x: std::bignum::bigint::BigInt
        y: std::bignum::bigint::BigInt
    }

    fun f(p: Point) -> std::void::Void {
        case p is Point(x) { }
    }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CasePatternVariantDestructureObjectAst,
    test_invalid_invalid_attribute,
    SppIdentifierUnknownError, R"(
    cls Point {
        x: std::bignum::bigint::BigInt
        y: std::bignum::bigint::BigInt
    }

    fun f(p: Point) -> std::void::Void {
        case p is Point(x, y, z) { }
    }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CasePatternVariantDestructureObjectAst,
    test_invalid_aliasing_attributes_use_old_name,
    SppIdentifierUnknownError, R"(
    cls Point1 {
        x: std::bignum::bigint::BigInt
        y: std::bignum::bigint::BigInt
    }

    fun f(p: Point1) -> std::void::Void {
        case p is Point1(x as x_value, ..) { let xxx = x }
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureObjectAst,
    test_valid_value_only, R"(
    cls Point {
        x: std::bignum::bigint::BigInt
        y: std::bignum::bigint::BigInt
    }

    fun f(p: Point) -> std::void::Void {
        case p is Point(x, y) { }
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureObjectAst,
    test_valid_value_and_unbound_multi_skip, R"(
    cls Point {
        x: std::bignum::bigint::BigInt
        y: std::bignum::bigint::BigInt
    }

    fun f(p: Point) -> std::void::Void {
        case p is Point(x, ..) { }
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureObjectAst,
    test_valid_multiple_branches, R"(
    cls Point1 {
        x: std::bignum::bigint::BigInt
        y: std::bignum::bigint::BigInt
    }

    cls Point2 {
        x: std::bignum::bigint::BigInt
        y: std::bignum::bigint::BigInt
    }

    fun f(p: Point1 or Point2) -> std::void::Void {
        case p of {
            is Point1(x, y) { }
            is Point2(x, y) { }
        }
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureObjectAst,
    test_valid_valid_value_and_alias, R"(
    cls Point1 {
        x: std::bignum::bigint::BigInt
        y: std::bignum::bigint::BigInt
    }

    fun f(p: Point1) -> std::void::Void {
        case p of {
            is Point1(x as x_value, ..) { let xxx = x_value }
            is Point1(y as y_value, ..) { let yyy = y_value }
        }
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureObjectAst,
    test_valid_nested_array_in_object, R"(
    cls Point {
        x: std::array::Arr[std::bignum::bigint::BigInt, 2_uz]
        y: std::bignum::bigint::BigInt
    }
    fun f(p: Point) -> std::void::Void {
        case p is Point(x=[a, b], y) { }
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureObjectAst,
    test_valid_nested_tuple_in_object, R"(
    cls Point {
        x: (std::bignum::bigint::BigInt, std::bignum::bigint::BigInt)
        y: std::bignum::bigint::BigInt
    }
    fun f(p: Point) -> std::void::Void {
        case p is Point(x=(a, b), y) { }
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureObjectAst,
    test_valid_nested_object_in_object, R"(
    cls Point {
        x: std::bignum::bigint::BigInt
        y: std::bignum::bigint::BigInt
    }

    cls Line {
        start: Point
        end: Point
    }

    fun f(p: Line) -> std::void::Void {
        case p is Line(start=Point(x as x1, y as y1), end=Point(x as x2, y as y2)) { }
    }
)");;
