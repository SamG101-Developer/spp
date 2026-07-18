#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableSingleIdentifierAliasAst,
    test_valid_object_attribute_alias, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let Point(x as a, y as b) = Point(x=1, y=2)
        let sum = a + b
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    LocalVariableSingleIdentifierAliasAst,
    test_valid_mutable_alias, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let Point(mut x as a, y) = Point(x=1, y=2)
        a = 5
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    LocalVariableSingleIdentifierAliasAst,
    test_invalid_original_attribute_name_not_bound,
    SppIdentifierUnknownError, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let Point(x as a, y) = Point(x=1, y=2)
        let z = x
    }
)");
