#include "../test_macros.hpp"
import testex;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureTupleAst,
    test_invalid_non_array_value,
    SppVariableTupleDestructureTupleTypeMismatchError, R"(
    fun f() -> std::void::Void {
        let (a, b) = 1
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureTupleAst,
    test_invalid_multiple_multi_skip,
    SppMultipleSkipMultiArgumentsError, R"(
    fun f() -> std::void::Void {
        let (a, .., .., b) = [1, 2, 3, 4]
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureTupleAst,
    test_invalid_array_missing_element,
    SppVariableTupleDestructureTupleSizeMismatchError, R"(
    fun f() -> std::void::Void {
        let (a, b) = (1, 2, 3)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureTupleAst,
    test_invalid_array_invalid_element,
    SppVariableTupleDestructureTupleSizeMismatchError, R"(
    fun f() -> std::void::Void {
        let (a, b, c) = (1, 2)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureTupleAst,
    test_valid_simple, R"(
    fun f() -> std::void::Void {
        let (a, b, c) = (1, 2, 3)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureTupleAst,
    test_valid_with_single_skip, R"(
    fun f() -> std::void::Void {
        let (mut a, _, mut c) = (1, 2, 3)
        a = 5
        c = 6
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureTupleAst,
    test_valid_with_multi_skip, R"(
    fun f() -> std::void::Void {
        let (a, .., c) = (1, 2, 3, 4, 5)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureTupleAst,
    test_valid_with_bound_multi_skip_start, R"(
    fun f() -> std::void::Void {
        let (a, ..mut b, c) = (1, 2, 3, 4)
        b = (5, 6)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureTupleAst,
    test_valid_with_bound_multi_skip_middle, R"(
    fun f() -> std::void::Void {
        let (..mut a, b) = (1, 2, 3, 4)
        a = (5, 6, 7)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureTupleAst,
    test_valid_with_bound_multi_skip_end, R"(
    fun f() -> std::void::Void {
        let (a, b, ..mut c) = (1, 2, 3, 4)
        c = (5, 6)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureTupleAst,
    test_valid_nested_array, R"(
    fun f() -> std::void::Void {
        let t = (1, 2)
        let ((a, mut b), c) = (t, [3, 4])
        b = 4
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureTupleAst,
    test_valid_nested_tuple, R"(
    fun f() -> std::void::Void {
        let t = (1, "2")
        let ((a, mut b), c) = (t, (3, "4"))
        b = "5"
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureTupleAst,
    test_valid_nested_object, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    fun f() -> std::void::Void {
        let points = (Point(x=1, y=2), Point(x=3, y=4))
        let (Point(x, mut y), ..) = points
        y = 5
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableDestructureTupleAst,
    test_valid_assign_to_uninitialized_parts, R"(
    fun f() -> std::void::Void {
        let (x, y): (std::boolean::Bool, std::boolean::Bool)
        x = true
        y = true
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureTupleAst,
    test_invalid_explicit_type_too_big,
    SppVariableTupleDestructureTupleSizeMismatchError, R"(
    fun f() -> std::void::Void {
        let (x, y): (std::boolean::Bool, std::boolean::Bool, std::boolean::Bool)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureTupleAst,
    test_invalid_explicit_type_too_small,
    SppVariableTupleDestructureTupleSizeMismatchError, R"(
    fun f() -> std::void::Void {
        let (x, y): (std::boolean::Bool,)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableDestructureTupleAst,
    test_invalid_assign_to_individual_part_wrong_type,
    SppTypeMismatchError, R"(
    fun f() -> std::void::Void {
        let (x, y): (std::boolean::Bool, std::boolean::Bool)
        x = "hello world"
    }
)");
