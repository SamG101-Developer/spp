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

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClassPrototypeAst,
    test_invalid_recursive_definition_between_three_classes,
    SppRecursiveTypeError, R"(
    cls A {
        a: B
    }

    cls B {
        a: C
    }

    cls C {
        a: A
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClassPrototypeAst,
    test_invalid_duplicate_class_name,
    SppIdentifierDuplicateError, R"(
    cls A {
        a: Str
    }

    cls A {
        b: Str
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClassPrototypeAst,
    test_valid_regular_class_definition, R"(
    cls A {
        a: B
    }

    cls B {
        a: Str
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClassPrototypeAst,
    test_valid_self_reference_via_heap_indirection, R"(
    cls A {
        a: Vec[A]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClassPrototypeAst,
    test_valid_empty_class, R"(
    cls A { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClassPrototypeAst,
    test_valid_type_generic_class_definition, R"(
    cls A[T] {
        a: T
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClassPrototypeAst,
    test_valid_comp_generic_class_definition, R"(
    cls A[T, cmp n: USize] {
        a: USize = n
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClassPrototypeAst,
    test_valid_visibility_annotation, R"(
    !public
    cls A {
        a: Str
    }
)");
