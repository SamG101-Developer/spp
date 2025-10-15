#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstIterExpression,
    test_invalid_gen_opt_missing_opt_branch,
    SppIterExpressionPatternMissingError, R"(
    use std::generator::GenOpt
    use std::string::Str
    use std::void::Void

    cor g() -> GenOpt[Str] {
        gen "hello"
        gen
    }

    fun f() -> Void {
        let generator = g()
        let value = generator.res()

        let x = iter value of {
            internal { 1 }
            !! { 2 }
        }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstIterExpression,
    test_invalid_gen_res_missing_res_branch,
    SppIterExpressionPatternMissingError, R"(
    use std::generator::GenRes
    use std::string::Str
    use std::void::Void

    cls Error { }

    cor g() -> GenRes[Str, Error] {
        gen "hello"
        gen Error()
    }

    fun f() -> Void {
        let generator = g()
        let value = generator.res()

        let x = iter value of {
            internal { 1 }
            !! { 2 }
        }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstIterExpression,
    test_invalid_gen_missing_val_branch,
    SppIterExpressionPatternMissingError, R"(
    use std::generator::Gen
    use std::string::Str
    use std::void::Void

    cor g() -> Gen[Str] {
        gen "hello"
    }

    fun f() -> Void {
        let generator = g()
        let value = generator.res()

        let x = iter value of {
            internal { 1 }
        }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstIterExpression,
    test_invalid_iter_pattern_incompatible_type_gen_opt,
    SppIterExpressionPatternIncompatibleError, R"(
    use std::generator::GenOpt
    use std::string::Str
    use std::void::Void

    cor g() -> GenOpt[Str] {
        gen "hello"
        gen
    }

    fun f() -> Void {
        let generator = g()
        let value = generator.res()

        let x = iter value of {
            !error { 1 }
        }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstIterExpression,
    test_invalid_iter_pattern_incompatible_type_gen_res,
    SppIterExpressionPatternIncompatibleError, R"(
    use std::generator::GenRes
    use std::string::Str
    use std::void::Void

    cls Error { }

    cor g() -> GenRes[Str, Error] {
        gen "hello"
        gen Error()
    }

    fun f() -> Void {
        let generator = g()
        let value = generator.res()

        let x = iter value of {
            _ { 1 }
        }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstIterExpression,
    test_invalid_iter_pattern_incompatible_type_gen_1,
    SppIterExpressionPatternIncompatibleError, R"(
    use std::generator::Gen
    use std::string::Str
    use std::void::Void

    cor g() -> Gen[Str] {
        gen "hello"
    }

    fun f() -> Void {
        let generator = g()
        let value = generator.res()

        let x = iter value of {
            !error { 1 }
        }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstIterExpression,
    test_invalid_iter_pattern_incompatible_type_gen_2,
    SppIterExpressionPatternIncompatibleError, R"(
    use std::generator::Gen
    use std::string::Str
    use std::void::Void

    cor g() -> Gen[Str] {
        gen "hello"
    }

    fun f() -> Void {
        let generator = g()
        let value = generator.res()

        let x = iter value of {
            _ { 1 }
        }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstIterExpression,
    test_invalid_iter_pattern_duplicate_type,
    SppIterExpressionPatternTypeDuplicateError, R"(
    use std::generator::Gen
    use std::string::Str
    use std::void::Void

    cor g() -> Gen[Str] {
        gen "hello"
    }

    fun f() -> Void {
        let generator = g()
        let value = generator.res()

        let x = iter value of {
            internal { 1 }
            internal { 2 }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstIterExpression,
    test_valid_iter_opt, R"(
    use std::generator::GenOpt
    use std::string::Str
    use std::void::Void

    cor g() -> GenOpt[Str] {
        gen "hello"
        gen
    }

    fun f() -> Void {
        let mut generator = g()
        let value = generator.res()

        iter value of {
            internal { 1 }
            _ { 2 }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstIterExpression,
    test_valid_iter_res, R"(
    use std::generator::GenRes
    use std::string::Str
    use std::void::Void

    cls Error { }

    cor g() -> GenRes[Str, Error] {
        gen "hello"
        gen Error()
    }

    fun f() -> Void {
        let mut generator = g()
        let value = generator.res()

        iter value of {
            internal { 1 }
            !error { 2 }
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstIterExpression,
    test_valid_iter_gen, R"(
    use std::generator::Gen
    use std::string::Str
    use std::void::Void

    cor g() -> Gen[Str] {
        gen "hello"
    }

    fun f() -> Void {
        let mut generator = g()
        let value = generator.res()

        iter value of {
            internal { 1 }
        }
    }
)");

