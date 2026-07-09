#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestMscVisibility,
    test_invalid_visibility_access_private_type_member_diff_ctx_same_mod,
    SppAccessViolationError, R"(
    cls A {
        !private a: Str
    }

    fun function(a: A) -> Void {
        let x = a.a
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestMscVisibility,
    test_invalid_visibility_access_private_type_member_diff_ctx_diff_module,
    SppAccessViolationError, R"(
    fun function(a: std::threading::mutex::Mutex) -> Void {
        let x = a.id
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestMscVisibility,
    test_invalid_visibility_access_private_type_member_same_ctx_diff_module,
    SppAccessViolationError, R"(
    sup std::threading::mutex::Mutex {
        fun function(&self) -> Void {
            let x = self.id
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestMscVisibility,
    test_valid_visibility_access_private_type_member_same_ctx_same_module, R"(
    cls A {
        !private a: Str
    }

    sup A {
        fun function(&self) -> Void {
            let x = self.a
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestMscVisibility,
    test_invalid_visibility_access_private_module_member,
    SppAccessViolationError, R"(
    fun f() -> Void {
        let x = std::console::stdout_stream::stdout_fd
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestMscVisibility,
    test_valid_visibility_access_protected_type_member_extended_ctx_same_module, R"(
    cls A {
        !protected a: Str
    }

    sup A {
        !virtual_method
        fun function(&self) -> Void { }
    }

    cls B { }

    sup B ext A {
        fun function(&self) -> Void {
            let x = self.a
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestMscVisibility,
    test_invalid_visibility_different_accessors_on_overload,
    SppFunctionOverloadVisibilityMismatchError, R"(
    !package fun function(a: U32) -> Void { }
    !private fun function(a: U16) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestMscVisibility,
    test_valid_visibility_access_public_type_member_diff_ctx_same_mod, R"(
    cls A {
        !public a: Str
    }

    fun function(a: A) -> Void {
        let x = a.a
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestMscVisibility,
    test_invalid_visibility_access_protected_type_member_diff_ctx_same_mod,
    SppAccessViolationError, R"(
    cls A {
        !protected a: Str
    }

    fun function(a: A) -> Void {
        let x = a.a
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestMscVisibility,
    test_valid_visibility_access_protected_type_member_same_ctx_same_module, R"(
    cls A {
        !protected a: Str
    }

    sup A {
        fun function(&self) -> Void {
            let x = self.a
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestMscVisibility,
    test_invalid_visibility_access_private_method_diff_ctx_same_mod,
    SppAccessViolationError, R"(
    cls A { }

    sup A {
        !private
        fun secret(&self) -> Void { }
    }

    fun function(a: A) -> Void {
        a.secret()
    }
)");
