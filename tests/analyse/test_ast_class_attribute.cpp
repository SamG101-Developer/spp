#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClassAttributeAst,
    test_invalid_duplicate_identifier,
    SppIdentifierDuplicateError, R"(
    cls A {
        a: std::string::Str
        a: std::string::Str
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClassAttributeAst,
    test_invalid_convention_mut,
    SppSecondClassBorrowViolationError, R"(
    cls A {
        a: &mut std::string::Str
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClassAttributeAst,
    test_invalid_convention_ref,
    SppSecondClassBorrowViolationError, R"(
    cls A {
        a: &std::string::Str
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClassAttributeAst,
    test_invalid_convention_mut_from_generic_substitution,
    SppSecondClassBorrowViolationError, R"(
    cls A[T] {
        a: T
    }

    fun f() -> std::void::Void {
        let a = A[&mut std::string::Str]()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClassAttributeAst,
    test_invalid_convention_ref_from_generic_substitution,
    SppSecondClassBorrowViolationError, R"(
    cls A[T] {
        a: T
    }

    fun f() -> std::void::Void {
        let a = A[&std::string::Str]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClassAttributeAst,
    test_valid_attributes, R"(
    cls A {
        a: std::string::Str
        b: std::string::Str
    }

    cls B {
        a: std::string::Str
        b: std::string::Str
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClassAttributeAst,
    test_valid_attributes_with_super_class, R"(
    cls A {
        a: std::string::Str
    }

    cls B {
        b: std::string::Str
    }

    cls C { }
    sup C ext A {}
    sup C ext B {}
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    ClassAttributeAst,
    test_invalid_default_value,
    SppTypeMismatchError, R"(
    cls A {
        a: std::string::Str = 1
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    ClassAttributeAst,
    test_valid_default_value, R"(
    cls A {
        a: std::string::Str = "Hello"
    }
)");
