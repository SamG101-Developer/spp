#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CoroutinePrototypeAst,
    test_invalid_return_type,
    SppCoroutineInvalidReturnTypeError, R"(
    cor c() -> std::void::Void { }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CoroutinePrototypeAst,
    test_invalid_convention_mismatch_1,
    SppYieldedTypeMismatchError, R"(
    cor c() -> std::generator::Gen[&mut std::number::S32] {
        gen 1
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CoroutinePrototypeAst,
    test_invalid_convention_mismatch_2,
    SppYieldedTypeMismatchError, R"(
    cor c() -> std::generator::Gen[&std::number::S32] {
        gen 1
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CoroutinePrototypeAst,
    test_invalid_convention_mismatch_3,
    SppYieldedTypeMismatchError, R"(
    cor c() -> std::generator::Gen[std::number::S32] {
        gen &mut 1
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CoroutinePrototypeAst,
    test_valid_return_type_ref_with_mut_coercion, R"(
    cor c() -> std::generator::Gen[&std::number::S32] {
        gen &mut 1
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CoroutinePrototypeAst,
    test_valid_return_type_mov, R"(
    cor c() -> std::generator::Gen[std::number::S32] {
        gen 1
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CoroutinePrototypeAst,
    test_valid_return_type_mut, R"(
    cor c() -> std::generator::Gen[&mut std::number::S32] {
        gen &mut 1
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CoroutinePrototypeAst,
    test_valid_return_type_ref, R"(
    cor c() -> std::generator::Gen[&std::number::S32] {
        gen &1
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CoroutinePrototypeAst,
    test_valid_pinned_borrows, R"(
    cor c(a: &mut std::boolean::Bool, b: &std::number::S32) -> std::generator::Gen[std::number::S32] { }
    fun f() -> std::void::Void {
        let (mut x, y) = (false, 123)
        c(&mut x, &y)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CoroutinePrototypeAst,
    test_valid_return_type_inheriting_generator, R"(
    cls MyType { }

    sup MyType ext std::generator::Gen[&std::number::S32] { }

    cor c() -> MyType {
        gen &1
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CoroutinePrototypeAst,
    test_invalid_return_type_inheriting_multiple_generators,
    SppExpressionAmbiguousGeneratorError, R"(
    cls MyType { }

    sup MyType ext std::generator::Gen[&std::number::S32] { }

    sup MyType ext std::generator::Gen[&std::string::Str] { }

    cor c() -> MyType {
        gen &1
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CoroutinePrototypeAst,
    test_invalid_contains_ret_statement_with_val,
    SppFunctionCoroutineContainsRetExprExpressionError, R"(
    cor c() -> std::generator::Gen[&std::number::S32] {
        ret 123
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    CoroutinePrototypeAst,
    test_valid_contains_ret_statement_no_val, R"(
    cor c() -> std::generator::Gen[&std::number::S32] {
        ret
    }
)")
