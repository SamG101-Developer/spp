#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CasePatternVariantDestructureTupleAst,
    test_invalid_multiple_multi_skip,
    SppMultipleSkipMultiArgumentsError, R"(
    fun f(p: (std::bignum::bigint::BigInt, std::bignum::bigint::BigInt)) -> std::void::Void {
        case p is [.., ..] { }
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CasePatternVariantDestructureTupleAst,
    test_invalid_missing_value,
    SppVariableTupleDestructureTupleSizeMismatchError, R"(
    fun f(p: (std::bignum::bigint::BigInt, std::bignum::bigint::BigInt)) -> std::void::Void {
        case p is (x) { }
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CasePatternVariantDestructureTupleAst,
    test_invalid_extra_value,
    SppVariableTupleDestructureTupleSizeMismatchError, R"(
    fun f(p: (std::bignum::bigint::BigInt, std::bignum::bigint::BigInt)) -> std::void::Void {
        case p is (x, y, z) { }
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureTupleAst,
    test_valid_value_only, R"(
    fun f(p: (std::bignum::bigint::BigInt, std::bignum::bigint::BigInt)) -> std::void::Void {
        case p is (x, y) { }
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureTupleAst,
    test_valid_value_and_single_skip, R"(
    fun f(p: (std::bignum::bigint::BigInt, std::bignum::bigint::BigInt)) -> std::void::Void {
        case p is (x, _) { }
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureTupleAst,
    test_valid_value_and_unbound_multi_skip, R"(
    fun f(p: (std::bignum::bigint::BigInt, std::bignum::bigint::BigInt)) -> std::void::Void {
        case p is (x, ..) { }
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureTupleAst,
    test_valid_value_and_bound_multi_skip, R"(
    fun f(p: (std::bignum::bigint::BigInt, std::bignum::bigint::BigInt)) -> std::void::Void {
        case p is (..x) { }
    }
)")



SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureTupleAst,
    test_valid_multiple_branches, R"(
    fun f(p: (std::bignum::bigint::BigInt, std::bignum::bigint::BigInt)) -> std::void::Void {
        case p of {
            is (x, y) { }
            is (x, ..) { }
        }
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureArrayAst,
    test_valid_nested_array_in_tuple, R"(
    fun f(p: (std::array::Arr[std::bignum::bigint::BigInt, 2_uz], std::array::Arr[std::bignum::bigint::BigInt, 2_uz])) -> std::void::Void {
        case p is ([a, b], [c, d]) { }
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureArrayAst,
    test_valid_nested_tuple_in_tuple, R"(
    fun f(p: ((std::bignum::bigint::BigInt, std::bignum::bigint::BigInt), (std::bignum::bigint::BigInt, std::bignum::bigint::BigInt))) -> std::void::Void {
        case p is ((a, b), (c, d)) { }
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureArrayAst,
    test_valid_nested_object_in_tuple, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }
    fun f(p: (Point, Point)) -> std::void::Void {
        case p is (Point(x as a, y as b), Point(x as c, y as d)) { }
    }
)")

