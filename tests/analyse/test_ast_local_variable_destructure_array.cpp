#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureArrayAst,
    test_invalid_non_array_value,
    SppVariableArrayDestructureArrayTypeMismatchError, R"(
    fun f() -> Void {
        let [a, b] = 1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureArrayAst,
    test_invalid_multiple_multi_skip,
    SppMultipleRestPatternsError, R"(
    fun f() -> Void {
        let [a, .., .., b] = [1, 2, 3, 4]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureArrayAst,
    test_invalid_array_missing_element,
    SppVariableArrayDestructureArraySizeMismatchError, R"(
    fun f() -> Void {
        let [a, b] = [1, 2, 3]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureArrayAst,
    test_invalid_array_invalid_element,
    SppVariableArrayDestructureArraySizeMismatchError, R"(
    fun f() -> Void {
        let [a, b, c] = [1, 2]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureArrayAst,
    test_valid_simple, R"(
    fun f() -> Void {
        let [a, b, c] = [1, 2, 3]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureArrayAst,
    test_valid_with_single_skip, R"(
    fun f() -> Void {
        let [mut a, _, mut c] = [1, 2, 3]
        a = 5
        c = 6
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureArrayAst,
    test_valid_with_multi_skip, R"(
    fun f() -> Void {
        let [a, .., c] = [1, 2, 3, 4, 5]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureArrayAst,
    test_valid_with_bound_multi_skip_start, R"(
    fun f() -> Void {
        let [a, ..mut b, c] = [1, 2, 3, 4]
        b = [5, 6]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureArrayAst,
    test_valid_with_bound_multi_skip_middle, R"(
    fun f() -> Void {
        let [..mut a, b] = [1, 2, 3, 4]
        a = [5, 6, 7]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureArrayAst,
    test_valid_with_bound_multi_skip_end, R"(
    fun f() -> Void {
        let [a, b, ..mut c] = [1, 2, 3, 4]
        c = [5, 6]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureArrayAst,
    test_valid_nested_array, R"(
    fun f() -> Void {
        let t = [1, 2]
        let [[a, mut b], c] = [t, [3, 4]]
        b = 4
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureArrayAst,
    test_valid_nested_tuple, R"(
    fun f() -> Void {
        let t = (1, Str::from("2"))
        let [(a, mut b), c] = [t, (3, "4")]
        b = "5"
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureArrayAst,
    test_valid_nested_object, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let points = [Point(x=1, y=2), Point(x=3, y=4)]
        let [Point(x, mut y), ..] = points
        y = 5
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureArrayAst,
    test_valid_assign_to_uninitialized_parts, R"(
    fun f() -> Void {
        let [x, y]: [Bool; 2_uz]
        x = true
        y = true
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureArrayAst,
    test_invalid_explicit_type_too_big,
    SppVariableArrayDestructureArraySizeMismatchError, R"(
    fun f() -> Void {
        let [x, y]: [Bool; 3_uz]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureArrayAst,
    test_invalid_explicit_type_too_small,
    SppVariableArrayDestructureArraySizeMismatchError, R"(
    fun f() -> Void {
        let [x, y]: [Bool; 1_uz]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureArrayAst,
    test_invalid_assign_to_individual_part_wrong_type,
    SppTypeMismatchError, R"(
    fun f() -> Void {
        let [x, y]: [Bool; 2_uz]
        x = "hello world"
    }
)");
