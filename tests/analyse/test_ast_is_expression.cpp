#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IsExpressionAst,
    test_invalid_incorrect_type_destructure,
    SppTypeMismatchError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }
    fun f() -> std::void::Void {
        let a: Point = Point(x=1, y=2)
        case a is std::string::Str(..) { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IsExpressionAst,
    test_invalid_incorrect_type_variant_destructure,
    SppTypeMismatchError, R"(
    fun f() -> std::void::Void {
        let a: std::string::Str or std::boolean::Bool = "hello"
        case a is std::number::S32() { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IsExpressionAst,
    test_invalid_incorrect_generic_destructure,
    SppTypeMismatchError, R"(
    cls Point[T] {
        x: T
        y: T
    }
    fun f() -> std::void::Void {
        let a: Point[std::number::S32] = Point[std::number::S32](x=1, y=2)
        case a is Point[std::string::Str](x, y) { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    IsExpressionAst,
    test_valid_is_expression_correct_type, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }
    fun f() -> std::void::Void {
        let a: Point = Point(x=1, y=2)
        case a is Point(x, y) { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    IsExpressionAst,
    test_valid_type_variant, R"(
    fun f() -> std::void::Void {
        let a: std::string::Str or std::boolean::Bool = "hello"
        case a is std::string::Str(..) { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    IsExpressionAst,
    test_valid_type_variant_with_inference, R"(
    cls Point[T] {
        x: T
        y: T
    }
    fun f() -> std::void::Void {
        let a: std::option::Opt[std::string::Str] = std::option::Some(val="hello world")
        case a is std::option::Some(mut val) { val = "bye" }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    IsExpressionAst,
    test_valid_type_generic, R"(
    cls Point[T] {
        x: T
        y: T
    }
    fun f() -> std::void::Void {
        let a: Point[std::number::S32] = Point[std::number::S32](x=1, y=2)
        case a is Point[std::number::S32](x, y) { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    IsExpressionAst,
    test_valid_type_generic_with_inference, R"(
    cls Point[T] {
        x: T
        y: T
    }
    fun f() -> std::void::Void {
        let a: Point[std::number::S32] = Point[std::number::S32](x=1, y=2)
        case a is Point(x, y) { }
    }
)");

