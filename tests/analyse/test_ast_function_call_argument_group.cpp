#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_duplicate_named_argument,
    SppIdentifierDuplicateError, R"(
    fun f(a: Bool, b: Bool) -> Void { }

    fun g() -> Void {
        f(a=true, a=false)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_argument_order,
    SppOrderInvalidError, R"(
    fun f(a: Bool, b: Bool) -> Void { }

    fun g() -> Void {
        f(a=true, false)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_tuple_expansion,
    SppExpansionOfNonTupleError, R"(
    fun f(a: Bool, b: Bool) -> Void { }

    fun g() -> Void {
        let x = 1
        f(..x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_tuple_expansion, R"(
    fun f(a: Bool, b: Bool) -> Void { }

    fun g() -> Void {
        let x = (true, false)
        f(..x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_mixed_positional_and_keyword_order, R"(
    fun f(a: Bool, b: Bool) -> Void { }

    fun g() -> Void {
        f(true, b=false)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_multiple_keyword_args, R"(
    fun f(a: Bool, b: Bool) -> Void { }

    fun g() -> Void {
        f(b=false, a=true)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mut_value_and_mut_attr,
    SppMemoryOverlapUsageError, R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &mut A, b: &mut Bool) -> Void { }

    fun g(mut a: A) -> Void {
        f(&mut a, &mut a.b)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mov_value_and_mov_attr,
    SppUninitializedMemoryUseError, R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: A, b: Bool) -> Void { }

    fun g(mut a: A) -> Void {
        f(a, a.b)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_ref_value_and_mov_attr,
    SppMemoryOverlapUsageError, R"(
    cls A {
        !public a: Str
        !public b: Str
    }

    fun f(a: &A, b: Str) -> Void { }

    fun g(a: A) -> Void {
        f(&a, a.b)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mut_value_and_mov_attr,
    SppMemoryOverlapUsageError, R"(
    cls A {
        !public a: Str
        !public b: Str
    }

    fun f(a: &mut A, b: Str) -> Void { }

    fun g(mut a: A) -> Void {
        f(&mut a, a.b)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_ref_value_and_mut_attr,
    SppMemoryOverlapUsageError, R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &A, b: &mut Bool) -> Void { }

    fun g(mut a: A) -> Void {
        f(&a, &mut a.b)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mut_value_and_ref_attr,
    SppMemoryOverlapUsageError, R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &mut A, b: &Bool) -> Void { }

    fun g(mut a: A) -> Void {
        f(&mut a, &a.b)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mov_value_and_mut_attr,
    SppUninitializedMemoryUseError, R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: A, b: &mut Bool) -> Void { }

    fun g(mut a: A) -> Void {
        f(a, &mut a.b)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mov_value_and_ref_attr,
    SppMemoryOverlapUsageError, R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &mut A, b: &Bool) -> Void { }

    fun g(mut a: A) -> Void {
        f(&mut a, &a.b)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mut_attr_and_mut_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &mut Bool, b: &mut A) -> Void { }

    fun g(mut a: A) -> Void {
        f(&mut a.b, &mut a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_ref_attr_and_mov_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &Bool, b: A) -> Void { }

    fun g(a: A) -> Void {
        f(&a.b, a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mut_attr_and_mov_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &mut Bool, b: A) -> Void { }

    fun g(mut a: A) -> Void {
        f(&mut a.b, a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_ref_attr_and_mut_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &Bool, b: &mut A) -> Void { }

    fun g(mut a: A) -> Void {
        f(&a.b, &mut a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mut_attr_and_ref_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &mut Bool, b: &A) -> Void { }

    fun g(mut a: A) -> Void {
        f(&mut a.b, &a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mov_attr_and_mut_value,
    SppPartiallyInitializedMemoryUseError, R"(
    cls A {
        !public a: Str
        !public b: Str
    }

    fun f(a: Str, b: &mut A) -> Void { }

    fun g(mut a: A) -> Void {
        f(a.b, &mut a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mov_attr_and_ref_value,
    SppPartiallyInitializedMemoryUseError, R"(
    cls A {
        !public a: Str
        !public b: Str
    }

    fun f(a: Str, b: &A) -> Void { }

    fun g(mut a: A) -> Void {
        f(a.b, &a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mut_value_and_mut_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &mut A, b: &mut A) -> Void { }

    fun g(mut a: A) -> Void {
        f(&mut a, &mut a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_ref_value_and_mov_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &A, b: A) -> Void { }

    fun g(a: A) -> Void {
        f(&a, a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mut_value_and_mov_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &mut A, b: A) -> Void { }

    fun g(mut a: A) -> Void {
        f(&mut a, a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_ref_value_and_mut_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &A, b: &mut A) -> Void { }

    fun g(mut a: A) -> Void {
        f(&a, &mut a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mut_value_and_ref_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &mut A, b: &A) -> Void { }

    fun g(mut a: A) -> Void {
        f(&mut a, &a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mov_value_and_mut_value,
    SppUninitializedMemoryUseError, R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: A, b: &mut A) -> Void { }

    fun g(mut a: A) -> Void {
        f(a, &mut a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mov_value_and_ref_value,
    SppUninitializedMemoryUseError, R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: A, b: &A) -> Void { }

    fun g(a: A) -> Void {
        f(a, &a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_ref_value_and_ref_attr,
    R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &A, b: &Bool) -> Void { }

    fun g(a: A) -> Void {
        f(&a, &a.b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_ref_attr_and_ref_value,
    R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &Bool, b: &A) -> Void { }

    fun g(a: A) -> Void {
        f(&a.b, &a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_ref_value_and_ref_value,
    R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &A, b: &A) -> Void { }

    fun g(a: A) -> Void {
        f(&a, &a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_non_overlap_mut_attr_ref_attr,
    R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &mut Bool, b: &Bool) -> Void { }

    fun g(mut a: A) -> Void {
        f(&mut a.a, &a.b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_ref_attr_and_mut_attr,
    R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &Bool, b: &mut Bool) -> Void { }

    fun g(mut a: A) -> Void {
        f(&a.a, &mut a.b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_mut_attr_and_mut_attr,
    R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &mut Bool, b: &mut Bool) -> Void { }

    fun g(mut a: A) -> Void {
        f(&mut a.a, &mut a.b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_ref_attr_and_ref_attr,
    R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &Bool, b: &Bool) -> Void { }

    fun g(a: A) -> Void {
        f(&a.a, &a.b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_mov_attr_and_ref_attr,
    R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: Bool, b: &Bool) -> Void { }

    fun g(a: A) -> Void {
        f(a.a, &a.b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_mov_attr_and_mut_attr,
    R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: Bool, b: &mut Bool) -> Void { }

    fun g(mut a: A) -> Void {
        f(a.a, &mut a.b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_ref_attr_and_mov_attr,
    R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &Bool, b: Bool) -> Void { }

    fun g(a: A) -> Void {
        f(&a.a, a.b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_mut_attr_and_mov_attr,
    R"(
    cls A {
        !public a: Bool
        !public b: Bool
    }

    fun f(a: &mut Bool, b: Bool) -> Void { }

    fun g(mut a: A) -> Void {
        f(&mut a.a, a.b)
    }
)");
