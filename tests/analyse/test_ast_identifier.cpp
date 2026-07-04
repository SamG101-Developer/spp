#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IdentifierAst,
    test_invalid_identifier,
    SppIdentifierUnknownError, R"(
    fun f() -> Void {
        let mut x = y
        x = z
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    IdentifierAst,
    test_valid_identifier, R"(
    fun f() -> Void {
        let mut x = 1
        let y = 2
        x = y
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IdentifierAst,
    test_invalid_self_identifier_in_free_function,
    SppSelfIdentifierInvalidContextError, R"(
    fun f() -> Void {
        let x = self
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    IdentifierAst,
    test_valid_self_identifier_in_method, R"(
    cls A { }
    sup A {
        fun f(self) -> Void {
            let x = self
        }
    }
)");
