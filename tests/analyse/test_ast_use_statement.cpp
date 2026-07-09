#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestUseStatementAst,
    test_valid_use_statement_reduction,
    R"(
    use std::bignum::bigint::BigInt
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestUseStatementAst,
    test_valid_use_statement_via_prelude_reduction_use_generic_1,
    R"(
    fun f[T](a: Vec[T]) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestUseStatementAst,
    test_valid_use_statement_via_prelude_reduction_use_generic_2,
    R"(
    fun f[T, cmp n: USize](a: Arr[T, n]) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestUseStatementAst,
    test_valid_use_statement_aliased_name_usable, R"(
    use std::bignum::bigint::BigInt
    fun f(a: BigInt) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUseStatementAst,
    test_invalid_use_statement_unknown_old_type,
    SppIdentifierUnknownError, R"(
    use std::number::NonExistentType
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUseStatementAst,
    test_invalid_use_statement_duplicate_of_prelude,
    SppIdentifierDuplicateError, R"(
    use std::boolean::Bool
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestUseStatementAst,
    test_invalid_use_statement_duplicate_explicit,
    SppIdentifierDuplicateError, R"(
    use std::bignum::bigint::BigInt
    use std::bignum::bigint::BigInt
)");
