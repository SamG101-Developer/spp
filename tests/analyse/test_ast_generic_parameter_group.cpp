#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstGenericParameterGroup,
    test_invalid_duplicate_type_identifier,
    SppIdentifierDuplicateError, R"(
    cls A[T, U, T] {}
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstGenericParameterGroup,
    test_invalid_duplicate_cmp_identifier,
    SppIdentifierDuplicateError, R"(
    cls A[cmp t: std::string::Str, cmp u: std::string::Str, cmp t: std::number::S32] {}
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstGenericParameterGroup,
    test_invalid_order_opt_req,
    SppOrderInvalidError, R"(
    cls A[T=std::string::Str, U] {}
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstGenericParameterGroup,
    test_invalid_order_var_req,
    SppOrderInvalidError, R"(
    cls A[..T, U] {}
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstGenericParameterGroup,
    test_invalid_order_var_opt,
    SppOrderInvalidError, R"(
    cls A[..T, U=std::string::Str] {}
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstGenericParameterGroup,
    test_valid_req, R"(
    cls A[T] {}
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstGenericParameterGroup,
    test_valid_opt, R"(
    cls A[T = std::string::Str] {}
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstGenericParameterGroup,
    test_valid_var, R"(
    cls A[..T] {}
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstGenericParameterGroup,
    test_valid_req_opt, R"(
    cls A[T, U = std::boolean::Bool] {}
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstGenericParameterGroup,
    test_valid_req_var, R"(
    cls A[T, ..U] {}
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstGenericParameterGroup,
    test_valid_opt_var, R"(
    cls A[T = std::string::Str, ..U] {}
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstGenericParameterGroup,
    test_valid_order_req_opt_var, R"(
    cls A[T, U=std::string::Str, ..V] {}
)");
