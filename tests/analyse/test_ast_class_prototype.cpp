#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClassPrototypeAst,
    test_invalid_recursive_definition_within_class,
    SppRecursiveTypeError, R"(
    cls A {
        a: A
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClassPrototypeAst,
    test_invalid_recursive_definition_between_classes,
    SppRecursiveTypeError, R"(
    cls A {
        a: B
    }

    cls B {
        a: A
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClassPrototypeAst,
    test_valid_regular_class_definition, R"(
    cls A {
        a: B
    }

    cls B {
        a: std::string::Str
    }
)");
