#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenWithExpressionAst,
    test_invalid_convention_mismatch_ref_vs_mut,
    SppTypeMismatchError, R"(
    cor foo() -> std::generator::Gen[&std::number::S32] {
        gen &1
    }

    cor bar() -> std::generator::Gen[&mut std::number::S32] {
        gen with foo()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenWithExpressionAst,
    test_invalid_convention_mismatch_mut_vs_mov,
    SppTypeMismatchError, R"(
    cor foo() -> std::generator::Gen[&mut std::number::S32] {
        gen &mut 1
    }

    cor bar() -> std::generator::Gen[std::number::S32] {
        gen with foo()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenWithExpressionAst,
    test_invalid_convention_mismatch_mov_vs_ref,
    SppTypeMismatchError, R"(
    cor foo() -> std::generator::Gen[std::number::S32] {
        gen 1
    }

    cor bar() -> std::generator::Gen[&std::number::S32] {
        gen with foo()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenWithExpressionAst,
    test_invalid_convention_mismatch_mov_vs_mut,
    SppTypeMismatchError, R"(
    cor foo() -> std::generator::Gen[std::number::S32] {
        gen 1
    }

    cor bar() -> std::generator::Gen[&mut std::number::S32] {
        gen with foo()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenWithExpressionAst,
    test_valid_convention_mismatch_mut_vs_ref_coerce,
    R"(
    cor foo() -> std::generator::Gen[&mut std::number::S32] {
        gen &mut 1
    }

    cor bar() -> std::generator::Gen[&std::number::S32] {
        gen with foo()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenWithExpressionAst,
    test_valid_convention_mut_coercion,
    R"(
    cor foo() -> std::generator::Gen[&mut std::number::S32] {
        gen &mut 1
    }

    cor bar() -> std::generator::Gen[&std::number::S32] {
        gen with foo()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenWithExpressionAst,
    test_valid_convention_match_ref,
    R"(
    cor foo() -> std::generator::Gen[&std::number::S32] {
        gen &1
    }

    cor bar() -> std::generator::Gen[&std::number::S32] {
        gen with foo()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenWithExpressionAst,
    test_valid_convention_match_mut,
    R"(
    cor foo() -> std::generator::Gen[&mut std::number::S32] {
        gen &mut 1
    }

    cor bar() -> std::generator::Gen[&mut std::number::S32] {
        gen with foo()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    GenWithExpressionAst,
    test_valid_convention_match_mov,
    R"(
    cor foo() -> std::generator::Gen[std::number::S32] {
        gen 1
    }

    cor bar() -> std::generator::Gen[std::number::S32] {
        gen with foo()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    GenWithExpressionAst,
    test_invalid_type_mismatch,
    SppTypeMismatchError, R"(
    cor foo() -> std::generator::Gen[std::number::S32] {
        gen 1
    }

    cor bar() -> std::generator::Gen[std::boolean::Bool] {
        gen with foo()
    }
)");
