#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    IdentifierAst,
    test_invalid_identifier,
    SppIdentifierUnknownError, R"(
    fun f() -> std::void::Void {
        let mut x = y
        x = z
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    IdentifierAst,
    test_valid_identifier, R"(
    fun f() -> std::void::Void {
        let mut x = 1
        let y = 2
        x = y
    }
)");
