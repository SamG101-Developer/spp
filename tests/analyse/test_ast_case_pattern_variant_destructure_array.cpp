#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CasePatternVariantDestructureArrayAst,
    test_invalid_multiple_multi_skip,
    SppMultipleSkipMultiArgumentsError, R"(
    fun f(p: std::array::Arr[std::bignum::bigint::BigInt, 2_uz]) -> std::void::Void {
        case p is [.., ..] { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CasePatternVariantDestructureArrayAst,
    test_invalid_missing_value,
    SppVariableArrayDestructureArraySizeMismatchError, R"(
    fun f(p: std::array::Arr[std::bignum::bigint::BigInt, 2_uz]) -> std::void::Void {
        case p is [x] { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CasePatternVariantDestructureArrayAst,
    test_invalid_extra_value,
    SppVariableArrayDestructureArraySizeMismatchError, R"(
    fun f(p: std::array::Arr[std::bignum::bigint::BigInt, 2_uz]) -> std::void::Void {
        case p is [x, y, z] { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureArrayAst,
    test_valid_value_only, R"(
    fun f(p: std::array::Arr[std::bignum::bigint::BigInt, 2_uz]) -> std::void::Void {
        case p is [x, y] { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureArrayAst,
    test_valid_value_and_single_skip, R"(
    fun f(p: std::array::Arr[std::bignum::bigint::BigInt, 2_uz]) -> std::void::Void {
        case p is [x, _] { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureArrayAst,
    test_valid_value_and_unbound_multi_skip, R"(
    fun f(p: std::array::Arr[std::bignum::bigint::BigInt, 2_uz]) -> std::void::Void {
        case p is [x, ..] { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureArrayAst,
    test_valid_value_and_bound_multi_skip, R"(
    fun f(p: std::array::Arr[std::bignum::bigint::BigInt, 2_uz]) -> std::void::Void {
        case p is [..x] { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureArrayAst,
    test_valid_multiple_branches, R"(
    fun f(p: std::array::Arr[std::bignum::bigint::BigInt, 2_uz]) -> std::void::Void {
        case p of {
            is [x, y] { }
            is [x, ..] { }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureArrayAst,
    test_valid_nested_array_in_array, R"(
    fun f(p: std::array::Arr[std::array::Arr[std::bignum::bigint::BigInt, 2_uz], 2_uz]) -> std::void::Void {
        case p is [[a, b], [c, d]] { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureArrayAst,
    test_valid_nested_tuple_in_array, R"(
    fun f(p: std::array::Arr[(std::bignum::bigint::BigInt, std::bignum::bigint::BigInt), 2_uz]) -> std::void::Void {
        case p is [(a, b), (c, d)] { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CasePatternVariantDestructureArrayAst,
    test_valid_nested_object_in_array, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }
    fun f(p: std::array::Arr[Point, 2_uz]) -> std::void::Void {
        case p is [Point(x=10, y), Point(x, y=20)] { }
    }
)");
