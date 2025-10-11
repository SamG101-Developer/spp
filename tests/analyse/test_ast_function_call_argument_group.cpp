#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_duplicate_named_argument,
    SppIdentifierDuplicateError, R"(
    fun f(a: std::boolean::Bool, b: std::boolean::Bool) -> std::void::Void { }

    fun g() -> std::void::Void {
        f(a=true, a=false)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_argument_order,
    SppOrderInvalidError, R"(
    fun f(a: std::boolean::Bool, b: std::boolean::Bool) -> std::void::Void { }

    fun g() -> std::void::Void {
        f(a=true, false)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_tuple_expansion,
    SppExpansionOfNonTupleError, R"(
    fun f(a: std::boolean::Bool, b: std::boolean::Bool) -> std::void::Void { }

    fun g() -> std::void::Void {
        let x = 1
        f(..x)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mut_value_and_mut_attr,
    SppMemoryOverlapUsageError, R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &mut A, b: &mut std::boolean::Bool) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(&mut a, &mut a.b)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mov_value_and_mov_attr,
    SppUninitializedMemoryUseError, R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: A, b: std::boolean::Bool) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(a, a.b)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_ref_value_and_mov_attr,
    SppMemoryOverlapUsageError, R"(
    cls A {
        a: std::string::Str
        b: std::string::Str
    }

    fun f(a: &A, b: std::string::Str) -> std::void::Void { }

    fun g(a: A) -> std::void::Void {
        f(&a, a.b)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mut_value_and_mov_attr,
    SppMemoryOverlapUsageError, R"(
    cls A {
        a: std::string::Str
        b: std::string::Str
    }

    fun f(a: &mut A, b: std::string::Str) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(&mut a, a.b)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_ref_value_and_mut_attr,
    SppMemoryOverlapUsageError, R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &A, b: &mut std::boolean::Bool) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(&a, &mut a.b)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mut_value_and_ref_attr,
    SppMemoryOverlapUsageError, R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &mut A, b: &std::boolean::Bool) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(&mut a, &a.b)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mov_value_and_mut_attr,
    SppUninitializedMemoryUseError, R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: A, b: &mut std::boolean::Bool) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(a, &mut a.b)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mov_value_and_ref_attr,
    SppMemoryOverlapUsageError, R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &mut A, b: &std::boolean::Bool) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(&mut a, &a.b)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mut_attr_and_mut_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &mut std::boolean::Bool, b: &mut A) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(&mut a.b, &mut a)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_ref_attr_and_mov_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &std::boolean::Bool, b: A) -> std::void::Void { }

    fun g(a: A) -> std::void::Void {
        f(&a.b, a)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mut_attr_and_mov_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &mut std::boolean::Bool, b: A) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(&mut a.b, a)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_ref_attr_and_mut_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &std::boolean::Bool, b: &mut A) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(&a.b, &mut a)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mut_attr_and_ref_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &mut std::boolean::Bool, b: &A) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(&mut a.b, &a)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mov_attr_and_mut_value,
    SppPartiallyInitializedMemoryUseError, R"(
    cls A {
        a: std::bignum::bigint::BigInt
        b: std::bignum::bigint::BigInt
    }

    fun f(a: std::bignum::bigint::BigInt, b: &mut A) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(a.b, &mut a)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mov_attr_and_ref_value,
    SppPartiallyInitializedMemoryUseError, R"(
    cls A {
        a: std::bignum::bigint::BigInt
        b: std::bignum::bigint::BigInt
    }

    fun f(a: std::bignum::bigint::BigInt, b: &A) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(a.b, &a)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mut_value_and_mut_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &mut A, b: &mut A) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(&mut a, &mut a)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_ref_value_and_mov_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &A, b: A) -> std::void::Void { }

    fun g(a: A) -> std::void::Void {
        f(&a, a)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mut_value_and_mov_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &mut A, b: A) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(&mut a, a)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_ref_value_and_mut_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &A, b: &mut A) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(&a, &mut a)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mut_value_and_ref_value,
    SppMemoryOverlapUsageError, R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &mut A, b: &A) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(&mut a, &a)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mov_value_and_mut_value,
    SppUninitializedMemoryUseError, R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: A, b: &mut A) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(a, &mut a)
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_invalid_overlap_mov_value_and_ref_value,
    SppUninitializedMemoryUseError, R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: A, b: &A) -> std::void::Void { }

    fun g(a: A) -> std::void::Void {
        f(a, &a)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_ref_value_and_ref_attr,
    R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &A, b: &std::boolean::Bool) -> std::void::Void { }

    fun g(a: A) -> std::void::Void {
        f(&a, &a.b)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_ref_attr_and_ref_value,
    R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &std::boolean::Bool, b: &A) -> std::void::Void { }

    fun g(a: A) -> std::void::Void {
        f(&a.b, &a)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_ref_value_and_ref_value,
    R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &A, b: &A) -> std::void::Void { }

    fun g(a: A) -> std::void::Void {
        f(&a, &a)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_non_overlap_mut_attr_ref_attr,
    R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &mut std::boolean::Bool, b: &std::boolean::Bool) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(&mut a.a, &a.b)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_ref_attr_and_mut_attr,
    R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &std::boolean::Bool, b: &mut std::boolean::Bool) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(&a.a, &mut a.b)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_mut_attr_and_mut_attr,
    R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &mut std::boolean::Bool, b: &mut std::boolean::Bool) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(&mut a.a, &mut a.b)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_ref_attr_and_ref_attr,
    R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &std::boolean::Bool, b: &std::boolean::Bool) -> std::void::Void { }

    fun g(a: A) -> std::void::Void {
        f(&a.a, &a.b)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_mov_attr_and_ref_attr,
    R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: std::boolean::Bool, b: &std::boolean::Bool) -> std::void::Void { }

    fun g(a: A) -> std::void::Void {
        f(a.a, &a.b)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_mov_attr_and_mut_attr,
    R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: std::boolean::Bool, b: &mut std::boolean::Bool) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(a.a, &mut a.b)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_ref_attr_and_mov_attr,
    R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &std::boolean::Bool, b: std::boolean::Bool) -> std::void::Void { }

    fun g(a: A) -> std::void::Void {
        f(&a.a, a.b)
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionCallArgumentGroupAst,
    test_valid_overlap_mut_attr_and_mov_attr,
    R"(
    cls A {
        a: std::boolean::Bool
        b: std::boolean::Bool
    }

    fun f(a: &mut std::boolean::Bool, b: std::boolean::Bool) -> std::void::Void { }

    fun g(mut a: A) -> std::void::Void {
        f(&mut a.a, a.b)
    }
)")
