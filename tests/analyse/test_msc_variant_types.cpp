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
    fun f(a: &StrView or U64 or Bool) -> Str {
        ret case a of {
            is &StrView(..) { Str::from(a) }
            else { Str::from("hello world") }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestVariantTypes,
    test_variant_including_a_borrowed_type_2,
    SppSecondClassBorrowViolationError, R"(
    fun f(a: Str or &mut U64 or Bool) -> Str {
        ret Str::from("hello")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_and_tuple_combination, R"(
    fun g(a: (Opt[Str], U64)) -> Str {
        ret Str::from("hello world")
    }

    fun f() -> Void {
        let t = (Some(val=Str::from("hello world")), 123_u64)
        let a = g(t)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_as_return_type, R"(
    fun f() -> Bool or Str {
        ret true
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_as_let_annotation, R"(
    fun f() -> Void {
        let x: Bool or Str = true
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_as_function_argument, R"(
    fun g(x: Bool or Str) -> Void { }

    fun f() -> Void {
        g(true)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_as_class_attribute, R"(
    cls A {
        !public x: Bool or Str
    }

    fun f() -> Void {
        let a = A(x=true)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_as_generic_argument, R"(
    fun f() -> Void {
        let v = Vec[Bool or Str]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_as_array_element, R"(
    fun f(mut a: [Bool or Str; 2_uz]) -> Void {
        a = [true, Str::from("hello")]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_as_repeated_array_element, R"(
    fun f(mut a: [Bool or Str; 2_uz]) -> Void {
        a = [true; 2_uz]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    DISABLED_test_sub_variant_as_function_argument, R"(
    fun g(x: Str or U64 or Bool) -> Void { }

    fun f(b: Str or U64) -> Void {
        g(b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    DISABLED_test_sub_variant_as_return_type, R"(
    fun f(b: Str or U64) -> Str or U64 or Bool {
        ret b
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    DISABLED_test_sub_variant_as_let_annotation, R"(
    fun f(b: Str or U64) -> Void {
        let x: Str or U64 or Bool = b
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    DISABLED_test_sub_variant_as_class_attribute, R"(
    cls A {
        !public x: Str or U64 or Bool
    }

    fun f(b: Str or U64) -> Void {
        let a = A(x=b)
    }
)");
