#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_1, R"(
    fun f(mut a: &StrView or U64 or Bool) -> Void {
        a = "hello world"
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_2, R"(
    fun f(mut a: Str or U64 or Bool) -> Void {
        a = 123_u64
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_3, R"(
    fun f(mut a: Str or U64 or Bool) -> Void {
        a = true
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_from_subset_variant_1, R"(
    fun f(mut a: Str or U64 or Bool, b: Str or U64) -> Void {
        a = b
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_from_subset_variant_2, R"(
    fun f(mut a: Str or U64 or Bool, b: Str or Bool) -> Void {
        a = b
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_from_subset_variant_3, R"(
    fun f(mut a: Str or U64 or Bool, b: U64 or Bool) -> Void {
        a = b
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_type_assign_from_equal_variant, R"(
    fun f(mut a: Str or U64 or Bool, b: Str or U64 or Bool) -> Void {
        a = b
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_collapse_arguments, R"(
    fun f(mut a: Str or U64 or Bool, b: Str or U64 or Bool or Bool) -> Void {
        a = b
        std::mem::ops::drop(a)
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

// A variant may hold a borrow. These asserted the opposite, which the language cannot afford: "view.spp" indexes
// through "Indexed[&T or None]" everywhere, and narrowing a variant to its borrowed alternative is how a borrow is
// read back out of one. The restriction that does hold is on a type alias - see
// "test_invalid_type_statement_old_type_convention_ref" - because an alias presents no convention of its own while
// the type it resolves to has one, and every convention comparison downstream then asks the wrong question.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_including_a_borrowed_type_1, R"(
    fun f(a: &StrView or U64 or Bool) -> Str {
        ret case a of {
            is &StrView(..) { Str::from(a) }
            else { Str::from("hello world") }
        }
    }
)");

// ...including alongside an owned alternative, which is what makes the variant itself owned and so still owed.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_including_a_borrowed_type_2, R"(
    fun f(a: Str or &mut U64 or Bool) -> Str {
        std::mem::ops::drop(a)
        ret Str::from("hello")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_and_tuple_combination, R"(
    fun g(a: (Opt[Str], U64)) -> Str {
        std::mem::ops::drop(a)
        ret Str::from("hello world")
    }

    fun f() -> Void {
        let t = (Some(val=Str::from("hello world")), 123_u64)
        let a = g(t)
        std::mem::ops::drop(a)
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
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_as_function_argument, R"(
    fun g(x: Bool or Str) -> Void {
        std::mem::ops::drop(x)
    }

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
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_as_generic_argument, R"(
    fun f() -> Void {
        let v = Vec[Bool or Str]()
        std::mem::ops::drop(v)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_as_array_element, R"(
    fun f(mut a: [Bool or Str; 2_uz]) -> Void {
        a = [true, Str::from("hello")]
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_variant_as_repeated_array_element, R"(
    fun f(mut a: [Bool or Str; 2_uz]) -> Void {
        a = [true; 2_uz]
        std::mem::ops::drop(a)
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

// A variant is the set of its members: two spellings in different orders are one type, so a generic bound through both
// does not conflict.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_valid_member_order_is_not_part_of_the_type, R"(
    fun f[T](a: T, b: T) -> Void {
        std::mem::ops::drop(a)
        std::mem::ops::drop(b)
    }

    fun g(x: S32 or Bool, y: Bool or S32) -> Void {
        f(x, y)
    }
)");

// And one layout: returned as the other spelling, the value needs no reordering of its tags.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_valid_return_as_the_other_member_order, R"(
    fun g(x: S32 or Bool) -> Bool or S32 {
        ret x
    }
)");

// A variant's identity reads its members off its one argument ("Variants", or a lone tuple), so an argument beside
// "Variants" never reaches it: it is refused by name, in either order. The parameter is shown in its own file (std's
// "variant.spp"), not past the end of the test's.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestVariantTypes,
    test_invalid_extra_named_argument_beside_variants,
    SppArgumentNameInvalidError, R"(
    fun f(x: std::variant::Var[Variants=(S32, Str), Extra=Bool]) -> Void { std::mem::ops::drop(x) }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestVariantTypes,
    test_invalid_extra_named_argument_before_variants,
    SppArgumentNameInvalidError, R"(
    fun f(x: std::variant::Var[Extra=Bool, Variants=(S32, Str)]) -> Void { std::mem::ops::drop(x) }
)");

// Two positional arguments are two members, the first a tuple: not a tuple of members beside an extra argument. In
// either order, one type.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_valid_positional_tuple_member_beside_another, R"(
    fun f(x: std::variant::Var[(S32, Str), Bool]) -> Void { std::mem::ops::drop(x) }
    fun g(y: std::variant::Var[Bool, (S32, Str)]) -> Void { f(y) }
)");

// A lone positional tuple is one member, the tuple, as a pack takes it: keyed before analysis ("Scope::InstanceIdOf")
// and analysed alike, so it is not the variant of the tuple's elements.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypes,
    test_valid_lone_tuple_argument_is_one_member, R"(
    fun f(x: std::variant::Var[(S32, Bool)]) -> Void { std::mem::ops::drop(x) }
    fun g(y: std::variant::Var[(S32, Bool)]) -> Void { f(y) }
    fun h() -> Void { f((1_s32, false)) }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestVariantTypes,
    test_invalid_lone_tuple_argument_is_not_its_elements,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(x: std::variant::Var[(S32, Bool)]) -> Void { std::mem::ops::drop(x) }
    fun g(y: S32 or Bool) -> Void { f(y) }
)");
