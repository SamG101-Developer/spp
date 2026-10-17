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

// "IsTypeRecursive" only follows class attributes, so a type that holds itself by value through anything else - a
// generic of its own, a tuple, an array, another class's generic, a variant - is not caught. The first four overflow
// the stack later on; the variant lowers to a zero-byte payload.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClassPrototypeAst,
    test_invalid_recursive_type_through_its_own_generic,
    SppRecursiveTypeError, R"(
    cls RecA[T] { !public a: RecA[T] }

    fun f(x: RecA[S32]) -> Void { std::mem::ops::drop(x) }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClassPrototypeAst,
    test_invalid_recursive_type_through_a_tuple,
    SppRecursiveTypeError, R"(
    cls RecB { !public a: (S32, RecB) }

    fun f(x: RecB) -> Void { std::mem::ops::drop(x) }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClassPrototypeAst,
    test_invalid_recursive_type_through_an_array,
    SppRecursiveTypeError, R"(
    cls RecC { !public a: Arr[RecC, 2_uz] }

    fun f(x: RecC) -> Void { std::mem::ops::drop(x) }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClassPrototypeAst,
    test_invalid_recursive_type_through_a_generic_wrapper,
    SppRecursiveTypeError, R"(
    cls RecWrap[T] { !public w: T }
    cls RecD { !public a: RecWrap[RecD] }

    fun f(x: RecD) -> Void { std::mem::ops::drop(x) }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClassPrototypeAst,
    test_invalid_recursive_type_through_a_variant,
    SppRecursiveTypeError, R"(
    cls RecE { !public a: RecE or S32 }

    fun f(x: RecE) -> Void { std::mem::ops::drop(x) }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClassPrototypeAst,
    test_invalid_recursive_type_through_an_option,
    SppRecursiveTypeError, R"(
    cls RecF { !public a: Opt[RecF] }

    fun f(x: RecF) -> Void {
        let RecF(a) = x
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClassPrototypeAst,
    test_valid_recursive_type_through_a_vector, R"(
    cls RecG { !public a: Vec[RecG] }

    fun f(x: RecG) -> Void { std::mem::ops::drop(x) }
)");
