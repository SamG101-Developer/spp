#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_1, R"(
    fun f(mut a: StrView or U64 or Bool) -> Void {
        a = "hello world"
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_2, R"(
    fun f(mut a: Str or U64 or Bool) -> Void {
        a = 123_u64
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_3, R"(
    fun f(mut a: Str or U64 or Bool) -> Void {
        a = true
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_from_subset_variant_1, R"(
    fun f(mut a: Str or U64 or Bool, b: Str or U64) -> Void {
        a = b
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_from_subset_variant_2, R"(
    fun f(mut a: Str or U64 or Bool, b: Str or Bool) -> Void {
        a = b
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_from_subset_variant_3, R"(
    fun f(mut a: Str or U64 or Bool, b: U64 or Bool) -> Void {
        a = b
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_from_equal_variant, R"(
    fun f(mut a: Str or U64 or Bool, b: Str or U64 or Bool) -> Void {
        a = b
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_collapse_arguments, R"(
    fun f(mut a: Str or U64 or Bool, b: Str or U64 or Bool or Bool) -> Void {
        a = b
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_mismatched_composite_type,
    SppTypeMismatchError, R"(
    fun f(mut a: Str or U64 or Bool) -> Void {
        a = 123_s64
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_from_superset_variant,
    SppTypeMismatchError, R"(
    fun f(mut a: Str or U64 or Bool, b: Str or U64 or Bool or U32) -> Void {
        a = b
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_from_invalid_variant_some_overlap,
    SppTypeMismatchError, R"(
    fun f(mut a: Str or U64 or Bool, b: Str or U64 or U32) -> Void {
        a = b
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestVariantTypes,
    test_variant_including_a_borrowed_type_1,
    SppSecondClassBorrowViolationError, R"(
    fun f(a: &StrView or U64 or Bool) -> StrView {
        ret case a of {
            is &StrView(..) { a@ }
            else { "hello world" }
        }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestVariantTypes,
    test_variant_including_a_borrowed_type_2,
    SppSecondClassBorrowViolationError, R"(
    fun f(a: Str or &mut U64 or Bool) -> Str {
        ret a
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_and_tuple_combination, R"(
    fun g(a: (Opt[StrView], U64)) -> StrView {
        ret "hello world"
    }

    fun f() -> Void {
        let t = (Some(val="hello world"), 123_u64)
        let a = g(t)
    }
)");
