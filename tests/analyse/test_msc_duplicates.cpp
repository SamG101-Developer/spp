#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDuplicateMembers_SupType,
    test_valid_superimposition_extension_type_statement_diff_levels, R"(
    cls A { }
    sup A {
        !public type X = std::number::S32
    }

    cls B { }
    sup B ext A {
        !public type X = std::string::Str
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDuplicateMembers_SupType,
    test_invalid_superimposition_extension_type_statement_same_levels,
    SppIdentifierDuplicateError, R"(
    cls A { }
    sup A {
        !public type X = std::number::S32
    }

    sup A {
        !public type X = std::string::Str
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDuplicateMembers_SupType,
    test_valid_superimposition_extension_type_statement_same_levels_via_inheritance, R"(
    cls B { }
    sup B {
        !public type X = std::number::S32
    }

    cls C { }
    sup C {
        !public type X = std::string::Str
    }

    cls A { }
    sup A ext B { }
    sup A ext C { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDuplicateMembers_SupType,
    test_invalid_superimposition_extension_type_statement_same_levels_via_inheritance_with_ambiguous_access_1,
    SppAmbiguousMemberAccessError, R"(
    cls B { }
    sup B {
        !public type X = std::number::S32
    }

    cls C { }
    sup C {
        !public type X = std::string::Str
    }

    cls A { }
    sup A ext B { }
    sup A ext C { }

    fun f() -> std::void::Void {
        let x: A::X
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDuplicateMembers_SupType,
    test_invalid_superimposition_extension_type_statement_same_levels_via_inheritance_with_ambiguous_access_2,
    SppAmbiguousMemberAccessError, R"(
    cls B { }
    sup B {
        !public type X = std::number::S32
    }

    cls C { }
    sup C {
        !public type X = std::string::Str
    }

    cls A { }
    sup A ext B { }
    sup A ext C { }

    fun f() -> std::void::Void {
        let x = A::X()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDuplicateMembers_SupType,
    test_valid_superimposition_extension_type_statement_same_levels_via_inheritance_with_unique_override, R"(
    cls B { }
    sup B {
        !public type X = std::number::S32
    }

    cls C { }
    sup C {
        !public type X = std::string::Str
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
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDuplicateMembers_SupCmp,
    test_valid_superimposition_extension_cmp_statement_diff_levels, R"(
    cls A { }
    sup A {
        !public cmp x: std::number::S32 = 123
    }

    cls B { }
    sup B ext A {
        !public cmp x: std::number::S32 = 456
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDuplicateMembers_SupCmp,
    test_invalid_superimposition_extension_cmp_statement_diff_levels_diff_types,
    SppSuperimpositionExtensionCmpStatementInvalidError, R"(
    cls A { }
    sup A {
        !public cmp x: std::number::S32 = 123
    }

    cls B { }
    sup B ext A {
        !public cmp x: std::string_view::StrView = "hello world"
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDuplicateMembers_SupCmp,
    test_invalid_superimposition_extension_cmp_statement_same_levels,
    SppIdentifierDuplicateError, R"(
    cls A { }
    sup A {
        !public cmp x: std::number::S32 = 123
    }

    sup A {
        !public cmp x: std::string_view::StrView = "hello world"
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDuplicateMembers_SupCmp,
    test_valid_superimposition_extension_cmp_statement_same_levels_via_inheritance, R"(
    cls B { }
    sup B {
        !public cmp x: std::number::S32 = 123
    }

    cls C { }
    sup C {
        !public cmp x: std::string_view::StrView = "hello world"
    }

    cls A { }
    sup A ext B { }
    sup A ext C { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDuplicateMembers_SupCmp,
    test_invalid_superimposition_extension_cmp_statement_same_levels_via_inheritance_with_ambiguous_access_1,
    SppAmbiguousMemberAccessError, R"(
    cls B { }
    sup B {
        !public cmp x: std::number::USize = 123_uz
    }

    cls C { }
    sup C {
        !public cmp x: std::boolean::Bool = true
    }

    cls A { }
    sup A ext B { }
    sup A ext C { }

    fun f() -> std::void::Void {
        let x = A::x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDuplicateMembers_SupCmp,
    test_invalid_superimposition_extension_cmp_statement_same_levels_via_extension_with_unique_override_diff_types,
    SppSuperimpositionExtensionCmpStatementInvalidError, R"(
    cls B { }
    sup B {
        !public cmp x: std::number::USize = 123_uz
    }

    cls C { }
    sup C {
        !public cmp x: std::boolean::Bool = true
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
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDuplicateMembers_SupClsAttr,
    test_valid_superimposition_extension_cls_attr_statement_diff_levels, R"(
    cls A {
        !public a: std::number::S32
    }

    cls B {
        !public a: std::string::Str
    }

    sup B ext A { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDuplicateMembers_SupClsAttr,
    test_invalid_superimposition_extension_cls_attr_statement_same_levels,
    SppIdentifierDuplicateError, R"(
    cls A {
        a: std::number::S32
        a: std::string::Str
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDuplicateMembers_SupClsAttr,
    test_valid_superimposition_extension_cls_attr_statement_same_levels_via_inheritance, R"(
    cls B {
        !public a: std::number::S32
    }

    cls C {
        !public a: std::string::Str
    }

    cls A { }
    sup A ext B { }
    sup A ext C { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDuplicateMembers_SupClsAttr,
    test_invalid_superimposition_extension_cls_attr_statement_same_levels_via_inheritance_with_ambiguous_access,
    SppAmbiguousMemberAccessError, R"(
    cls B {
        !public a: std::number::S32
    }

    cls C {
        !public a: std::string::Str
    }

    cls A { }
    sup A ext B { }
    sup A ext C { }

    fun f() -> std::void::Void {
        let x = A().a
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDuplicateMembers_SupClsAttr,
    test_valid_superimposition_extension_cls_attr_statement_same_levels_via_inheritance_with_unique_override, R"(
    cls B {
        !public a: std::number::S32
    }

    cls C {
        !public a: std::string::Str
    }

    cls A {
        !public a: std::boolean::Bool
    }

    sup A ext B { }
    sup A ext C { }

    fun f() -> std::void::Void {
        let x = A().a
    }
)");

// Methods (functions in a sup block) at the SAME level: identical signatures conflict, but different
// signatures are a valid overload set (overriding across levels is covered in test_msc_overrides).
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDuplicateMembers_SupFunction,
    test_invalid_superimposition_function_same_levels_identical_signature,
    SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        !public fun f(&self) -> std::void::Void { }
    }

    sup A {
        !public fun f(&self) -> std::void::Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDuplicateMembers_SupFunction,
    test_invalid_superimposition_function_same_block_identical_signature,
    SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        !public fun f(&self) -> std::void::Void { }
        !public fun f(&self) -> std::void::Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestDuplicateMembers_SupFunction,
    test_valid_superimposition_function_same_levels_different_signature, R"(
    cls A { }
    sup A {
        !public fun f(&self) -> std::void::Void { }
    }

    sup A {
        !public fun f(&self, a: std::boolean::Bool) -> std::void::Void { }
    }
)");

// Two coroutines with identical signatures conflict just like two subroutines do.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestDuplicateMembers_SupFunction,
    test_invalid_superimposition_coroutine_same_levels_identical_signature,
    SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        !public cor c(&self) -> std::generator::Gen[std::boolean::Bool] { }
    }

    sup A {
        !public cor c(&self) -> std::generator::Gen[std::boolean::Bool] { }
    }
)");
