#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CoroutinePrototypeAst,
    test_invalid_return_type,
    SppExpressionNotGeneratorError, R"(
    cor c() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CoroutinePrototypeAst,
    test_invalid_convention_mismatch_1,
    SppYieldedTypeMismatchError, R"(
    cor c() -> Gen[&mut S32] {
        gen 1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CoroutinePrototypeAst,
    test_invalid_convention_mismatch_2,
    SppYieldedTypeMismatchError, R"(
    cor c() -> Gen[&S32] {
        gen 1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CoroutinePrototypeAst,
    test_invalid_convention_mismatch_3,
    SppYieldedTypeMismatchError, R"(
    cor c() -> Gen[S32] {
        gen &mut 1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CoroutinePrototypeAst,
    test_valid_return_type_ref_with_mut_coercion, R"(
    cor c() -> Gen[&S32] {
        gen &mut 1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CoroutinePrototypeAst,
    test_valid_return_type_mov, R"(
    cor c() -> Gen[S32] {
        gen 1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CoroutinePrototypeAst,
    test_valid_return_type_mut, R"(
    cor c() -> Gen[&mut S32] {
        gen &mut 1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CoroutinePrototypeAst,
    test_valid_return_type_ref, R"(
    cor c() -> Gen[&S32] {
        gen &1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CoroutinePrototypeAst,
    test_valid_pinned_borrows, R"(
    cor c(a: &mut Bool, b: &S32) -> Gen[S32] { }
    fun f() -> Void {
        let (mut x, y) = (false, 123)
        c(&mut x, &y)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CoroutinePrototypeAst,
    test_valid_return_type_gen_once, R"(
    cor c() -> GenOnce[S32] {
        gen 1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CoroutinePrototypeAst,
    test_valid_return_type_gen_with_send_type, R"(
    cor c() -> Gen[S32, Bool] {
        gen 1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CoroutinePrototypeAst,
    test_valid_coroutine_method, R"(
    cls MyType { }
    sup MyType {
        cor iter(&self) -> Gen[S32] {
            gen 1
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CoroutinePrototypeAst,
    test_valid_generic_coroutine, R"(
    cor c[T](x: T) -> Gen[T] {
        gen x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CoroutinePrototypeAst,
    test_invalid_non_generator_class_return_type,
    SppExpressionNotGeneratorError, R"(
    cls MyType { }
    cor c() -> MyType {
        gen 1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CoroutinePrototypeAst,
    test_valid_return_type_inheriting_generator, R"(
    cls MyType { }

    sup MyType ext Gen[&S32] { }

    cor c() -> MyType {
        gen &1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CoroutinePrototypeAst,
    test_invalid_return_type_inheriting_multiple_generators,
    SppExpressionAmbiguousGeneratorError, R"(
    cls MyType { }

    sup MyType ext Gen[&S32] { }

    sup MyType ext Gen[&Str] { }

    cor c() -> MyType {
        gen &1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    CoroutinePrototypeAst,
    test_invalid_contains_ret_statement_with_val,
    SppCoroutineContainsReturnStatementError, R"(
    cor c() -> Gen[&S32] {
        ret 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CoroutinePrototypeAst,
    test_valid_contains_ret_statement_no_val, R"(
    cor c() -> Gen[&S32] {
        ret
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    CoroutinePrototypeAst,
    test_auto_unwrap_vector, R"(
    fun f() -> Void {
        let mut vec = Vec[StrView]()
        vec.push_back("hello")
        vec.push_back("world")

        let mut elem1 = vec.index_ref(0_uz)
        let mut elem2 = vec.index_ref(1_uz)
        elem2 = elem1
    }
)");
