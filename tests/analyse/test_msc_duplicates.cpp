#include "../test_macros.hpp"
import testex;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDuplicateMembers_SupType,
    test_valid_superimposition_extension_type_statement_diff_levels, R"(
    cls A { }
    sup A {
        type X = std::number::S32
    }

    cls B { }
    sup B ext A {
        type X = std::string::Str
    }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDuplicateMembers_SupType,
    test_invalid_superimposition_extension_type_statement_same_levels,
    SppIdentifierDuplicateError, R"(
    cls A { }
    sup A {
        type X = std::number::S32
    }

    sup A {
        type X = std::string::Str
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDuplicateMembers_SupType,
    test_valid_superimposition_extension_type_statement_same_levels_via_inheritance, R"(
    cls B { }
    sup B {
        type X = std::number::S32
    }

    cls C { }
    sup C {
        type X = std::string::Str
    }

    cls A { }
    sup A ext B { }
    sup A ext C { }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDuplicateMembers_SupType,
    test_invalid_superimposition_extension_type_statement_same_levels_via_inheritance_with_ambiguous_access_1,
    SppAmbiguousMemberAccessError, R"(
    cls B { }
    sup B {
        type X = std::number::S32
    }

    cls C { }
    sup C {
        type X = std::string::Str
    }

    cls A { }
    sup A ext B { }
    sup A ext C { }

    fun f() -> std::void::Void {
        let x: A::X
    }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDuplicateMembers_SupType,
    test_invalid_superimposition_extension_type_statement_same_levels_via_inheritance_with_ambiguous_access_2,
    SppAmbiguousMemberAccessError, R"(
    cls B { }
    sup B {
        type X = std::number::S32
    }

    cls C { }
    sup C {
        type X = std::string::Str
    }

    cls A { }
    sup A ext B { }
    sup A ext C { }

    fun f() -> std::void::Void {
        let x = A::X()
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDuplicateMembers_SupType,
    test_valid_superimposition_extension_type_statement_same_levels_via_inheritance_with_unique_override, R"(
    cls B { }
    sup B {
        type X = std::number::S32
    }

    cls C { }
    sup C {
        type X = std::string::Str
    }

    cls A { }
    sup A ext B { }
    sup A ext C { }
    sup A {
        type X = std::boolean::Bool
    }

    fun f() -> std::void::Void {
        let x = A::X()
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDuplicateMembers_SupCmp,
    test_valid_superimposition_extension_cmp_statement_diff_levels, R"(
    cls A { }
    sup A {
        cmp x: std::number::S32 = 123
    }

    cls B { }
    sup B ext A {
        cmp x: std::string::Str = "hello world"
    }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDuplicateMembers_SupCmp,
    test_invalid_superimposition_extension_cmp_statement_same_levels,
    SppIdentifierDuplicateError, R"(
    cls A { }
    sup A {
        cmp x: std::number::S32 = 123
    }

    sup A {
        cmp x: std::string::Str = "hello world"
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDuplicateMembers_SupCmp,
    test_valid_superimposition_extension_cmp_statement_same_levels_via_inheritance, R"(
    cls B { }
    sup B {
        cmp x: std::number::S32 = 123
    }

    cls C { }
    sup C {
        cmp x: std::string::Str = "hello world"
    }

    cls A { }
    sup A ext B { }
    sup A ext C { }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDuplicateMembers_SupCmp,
    test_invalid_superimposition_extension_cmp_statement_same_levels_via_inheritance_with_ambiguous_access_1,
    SppAmbiguousMemberAccessError, R"(
    cls B { }
    sup B {
        cmp x: std::number::USize = 123_uz
    }

    cls C { }
    sup C {
        cmp x: std::boolean::Bool = true
    }

    cls A { }
    sup A ext B { }
    sup A ext C { }

    fun f() -> std::void::Void {
        let x = A::x
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDuplicateMembers_SupCmp,
    test_valid_superimposition_extension_cmp_statement_same_levels_via_inheritance_with_unique_override, R"(
    cls B { }
    sup B {
        cmp x: std::number::USize = 123_uz
    }

    cls C { }
    sup C {
        cmp x: std::boolean::Bool = true
    }

    cls A { }
    sup A ext B { }
    sup A ext C { }
    sup A {
        cmp x: std::number::U64 = 123_u64
    }

    fun f() -> std::void::Void {
        let x = A::x
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDuplicateMembers_SupClsAttr,
    test_valid_superimposition_extension_cls_attr_statement_diff_levels, R"(
    cls A {
        a: std::number::S32
    }

    cls B {
        a: std::string::Str
    }

    sup B ext A { }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDuplicateMembers_SupClsAttr,
    test_invalid_superimposition_extension_cls_attr_statement_same_levels,
    SppIdentifierDuplicateError, R"(
    cls A {
        a: std::number::S32
        a: std::string::Str
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDuplicateMembers_SupClsAttr,
    test_valid_superimposition_extension_cls_attr_statement_same_levels_via_inheritance, R"(
    cls B {
        a: std::number::S32
    }

    cls C {
        a: std::string::Str
    }

    cls A { }
    sup A ext B { }
    sup A ext C { }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDuplicateMembers_SupClsAttr,
    test_invalid_superimposition_extension_cls_attr_statement_same_levels_via_inheritance_with_ambiguous_access,
    SppAmbiguousMemberAccessError, R"(
    cls B {
        a: std::number::S32
    }

    cls C {
        a: std::string::Str
    }

    cls A { }
    sup A ext B { }
    sup A ext C { }

    fun f() -> std::void::Void {
        let x = A().a
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDuplicateMembers_SupClsAttr,
    test_valid_superimposition_extension_cls_attr_statement_same_levels_via_inheritance_with_unique_override, R"(
    cls B {
        a: std::number::S32
    }

    cls C {
        a: std::string::Str
    }

    cls A {
        a: std::boolean::Bool
    }

    sup A ext B { }
    sup A ext C { }

    fun f() -> std::void::Void {
        let x = A().a
    }
)");;
