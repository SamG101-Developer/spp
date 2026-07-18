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
    cls A[cmp t: Str, cmp u: Str, cmp t: S32] {}
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstGenericParameterGroup,
    test_invalid_order_opt_req_type,
    SppOrderInvalidError, R"(
    cls A[T=Str, U] {}
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstGenericParameterGroup,
    test_invalid_order_var_req_type,
    SppOrderInvalidError, R"(
    cls A[..T, U] {}
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstGenericParameterGroup,
    test_invalid_order_var_opt_type,
    SppOrderInvalidError, R"(
    cls A[..T, U=Str] {}
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstGenericParameterGroup,
    test_valid_req, R"(
    cls A[T] {}
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstGenericParameterGroup,
    test_valid_opt, R"(
    cls A[T = Str] {}
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstGenericParameterGroup,
    test_valid_var, R"(
    cls A[..T] {}
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstGenericParameterGroup,
    test_valid_req_opt, R"(
    cls A[T, U = Bool] {}
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstGenericParameterGroup,
    test_valid_req_var, R"(
    cls A[T, ..U] {}
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstGenericParameterGroup,
    test_valid_opt_var, R"(
    cls A[T = Str, ..U] {}
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstGenericParameterGroup,
    test_valid_order_req_opt_var, R"(
    cls A[T, U=Str, ..V] {}
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AstGenericParameterGroup,
    test_valid_mixed_type_and_comp, R"(
    cls A[T, cmp n: USize] {}
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstGenericParameterGroup,
    test_invalid_order_opt_req_comp,
    SppOrderInvalidError, R"(
    cls A[cmp n: StrView = "hello", cmp m: S32] {}
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstGenericParameterGroup,
    test_invalid_order_var_req_comp,
    SppOrderInvalidError, R"(
    cls A[cmp ..n: StrView, cmp m: S32] {}
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AstGenericParameterGroup,
    test_invalid_order_var_opt_comp,
    SppOrderInvalidError, R"(
    cls A[cmp ..n: StrView, cmp m: StrView = "hello"] {}
)");
