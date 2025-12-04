#include "../test_macros.hpp"
import testex;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestUseStatementAst,
    test_valid_use_statement_reduction,
    R"(
    use std::bignum::bigint::BigInt
    use std::string::Str
    use std::option::Opt
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestUseStatementAst,
    test_valid_use_statement_reduction_use_generic_1,
    R"(
    use std::vector::Vec

    fun f[T](a: Vec[T]) -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestUseStatementAst,
    test_valid_use_statement_reduction_use_generic_2,
    R"(
    use std::array::Arr
    use std::void::Void
    use std::number::USize

    fun f[T, cmp n: USize](a: Arr[T, n]) -> Void { }
)");
