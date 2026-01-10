#include "../test_macros.hpp"


/**
class TestVariantTypes(CustomTestCase):
    @should_pass_compilation()
    def test_variant_type_assign_1(self):
        """
        use std::boolean::Bool
        use std::number::U64
        use std::string::Str
        use std::void::Void

        fun f(mut a: Str or U64 or Bool) -> Void {
            a = "hello world"
        }
        """

    @should_pass_compilation()
    def test_variant_type_assign_2(self):
        """
        use std::boolean::Bool
        use std::number::U64
        use std::string::Str
        use std::void::Void

        fun f(mut a: Str or U64 or Bool) -> Void {
            a = 123_u64
        }
        """

    @should_pass_compilation()
    def test_variant_type_assign_3(self):
        """
        use std::boolean::Bool
        use std::number::U64
        use std::string::Str
        use std::void::Void

        fun f(mut a: Str or U64 or Bool) -> Void {
            a = true
        }
        """

    @should_pass_compilation
    def test_variant_type_assign_from_subset_variant_1(self):
        """
        use std::boolean::Bool
        use std::number::U64
        use std::string::Str
        use std::void::Void

        fun f(mut a: Str or U64 or Bool, b: Str or U64) -> Void {
            a = b
        }
        """

    @should_pass_compilation
    def test_variant_type_assign_from_subset_variant_2(self):
        """
        use std::boolean::Bool
        use std::number::U64
        use std::string::Str
        use std::void::Void

        fun f(mut a: Str or U64 or Bool, b: Str or Bool) -> Void {
            a = b
        }
        """

    @should_pass_compilation
    def test_variant_type_assign_from_subset_variant_3(self):
        """
        use std::boolean::Bool
        use std::number::U64
        use std::string::Str
        use std::void::Void

        fun f(mut a: Str or U64 or Bool, b: U64 or Bool) -> Void {
            a = b
        }
        """

    @should_pass_compilation
    def test_variant_type_assign_from_equal_variant(self):
        """
        use std::boolean::Bool
        use std::number::U64
        use std::string::Str
        use std::void::Void

        fun f(mut a: Str or U64 or Bool, b: Str or U64 or Bool) -> Void {
            a = b
        }
        """

    @should_pass_compilation()
    def test_variant_collapse_arguments(self):
        """
        use std::boolean::Bool
        use std::number::U64
        use std::string::Str
        use std::void::Void

        fun f(mut a: Str or U64 or Bool, b: Str or U64 or Bool or Bool) -> Void {
            a = b
        }
        """

    @should_fail_compilation(SemanticErrors.TypeMismatchError)
    def test_variant_type_assign_mismatched_composite_type(self):
        """
        use std::boolean::Bool
        use std::number::U64
        use std::string::Str
        use std::void::Void

        fun f(mut a: Str or U64 or Bool) -> Void {
            a = 123_s64
        }
        """

    @should_fail_compilation(SemanticErrors.TypeMismatchError)
    def test_variant_type_assign_from_superset_variant(self):
        """
        use std::boolean::Bool
        use std::number::U32
        use std::number::U64
        use std::string::Str
        use std::void::Void

        fun f(mut a: Str or U64 or Bool, b: Str or U64 or Bool or U32) -> Void {
            a = b
        }
        """

    @should_fail_compilation(SemanticErrors.TypeMismatchError)
    def test_variant_type_assign_from_invalid_variant_some_overlap(self):
        """
        use std::boolean::Bool
        use std::number::U32
        use std::number::U64
        use std::string::Str
        use std::void::Void

        fun f(mut a: Str or U64 or Bool, b: Str or U64 or U32) -> Void {
            a = b
        }
        """

    @should_fail_compilation(SemanticErrors.InvalidConventionLocationError)
    def test_variant_including_a_borrowed_type_1(self):
        """
        use std::boolean::Bool
        use std::number::U64
        use std::string::Str

        fun f(a: &Str or U64 or Bool) -> Str {
            ret a
        }
        """

    @should_fail_compilation(SemanticErrors.InvalidConventionLocationError)
    def test_variant_including_a_borrowed_type_2(self):
        """
        use std::boolean::Bool
        use std::number::U64
        use std::string::Str

        fun f(a: Str or &mut U64 or Bool) -> Str {
            ret a
        }
        """

    @should_pass_compilation()
    def test_variant_and_tuple_combination(self):
        """
        use std::option::Opt
        use std::option::Some
        use std::number::U64
        use std::string::Str
        use std::void::Void

        fun g(a: (Opt[Str], U64)) -> Str {
            ret "hello world"
        }

        fun f() -> Void {
            let t = (Some(val="hello world"), 123_u64)
            let a = g(t)
        }
        """
*/


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_1, R"(
    use std::boolean::Bool
    use std::number::U64
    use std::string::Str
    use std::void::Void

    fun f(mut a: Str or U64 or Bool) -> Void {
        a = "hello world"
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_2, R"(
    use std::boolean::Bool
    use std::number::U64
    use std::string::Str
    use std::void::Void

    fun f(mut a: Str or U64 or Bool) -> Void {
        a = 123_u64
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_3, R"(
    use std::boolean::Bool
    use std::number::U64
    use std::string::Str
    use std::void::Void

    fun f(mut a: Str or U64 or Bool) -> Void {
        a = true
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_from_subset_variant_1, R"(
    use std::boolean::Bool
    use std::number::U64
    use std::string::Str
    use std::void::Void

    fun f(mut a: Str or U64 or Bool, b: Str or U64) -> Void {
        a = b
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_from_subset_variant_2, R"(
    use std::boolean::Bool
    use std::number::U64
    use std::string::Str
    use std::void::Void

    fun f(mut a: Str or U64 or Bool, b: Str or Bool) -> Void {
        a = b
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_from_subset_variant_3, R"(
    use std::boolean::Bool
    use std::number::U64
    use std::string::Str
    use std::void::Void

    fun f(mut a: Str or U64 or Bool, b: U64 or Bool) -> Void {
        a = b
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_from_equal_variant, R"(
    use std::boolean::Bool
    use std::number::U64
    use std::string::Str
    use std::void::Void

    fun f(mut a: Str or U64 or Bool, b: Str or U64 or Bool) -> Void {
        a = b
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_collapse_arguments, R"(
    use std::boolean::Bool
    use std::number::U64
    use std::string::Str
    use std::void::Void

    fun f(mut a: Str or U64 or Bool, b: Str or U64 or Bool or Bool) -> Void {
        a = b
    }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_mismatched_composite_type,
    SppTypeMismatchError, R"(
    use std::boolean::Bool
    use std::number::U64
    use std::string::Str
    use std::void::Void

    fun f(mut a: Str or U64 or Bool) -> Void {
        a = 123_s64
    }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_from_superset_variant,
    SppTypeMismatchError, R"(
    use std::boolean::Bool
    use std::number::U32
    use std::number::U64
    use std::string::Str
    use std::void::Void

    fun f(mut a: Str or U64 or Bool, b: Str or U64 or Bool or U32) -> Void {
        a = b
    }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_from_invalid_variant_some_overlap,
    SppTypeMismatchError, R"(
    use std::boolean::Bool
    use std::number::U32
    use std::number::U64
    use std::string::Str
    use std::void::Void

    fun f(mut a: Str or U64 or Bool, b: Str or U64 or U32) -> Void {
        a = b
    }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestVariantTypes,
    test_variant_including_a_borrowed_type_1,
    SppSecondClassBorrowViolationError, R"(
    use std::boolean::Bool
    use std::number::U64
    use std::string::Str

    fun f(a: &Str or U64 or Bool) -> Str {
        ret a
    }
)");;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestVariantTypes,
    test_variant_including_a_borrowed_type_2,
    SppSecondClassBorrowViolationError, R"(
    use std::boolean::Bool
    use std::number::U64
    use std::string::Str

    fun f(a: Str or &mut U64 or Bool) -> Str {
        ret a
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_and_tuple_combination, R"(
    use std::option::Opt
    use std::option::Some
    use std::number::U64
    use std::string::Str
    use std::void::Void

    fun g(a: (Opt[Str], U64)) -> Str {
        ret "hello world"
    }

    fun f() -> Void {
        let t = (Some(val="hello world"), 123_u64)
        let a = g(t)
    }
)");;
