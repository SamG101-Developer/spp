#include "../test_macros.hpp"

// Narrowing a variant into a variant: a case pattern that names several of the subject's members at once, such as
// "is Opt[S32](..)" against "Some[S32] or None or Bool". The pattern is not one member, so it matches any of them and
// binds the value re-tagged as the narrower variant ("codegen::NarrowVariant"). The widening direction - assigning a
// narrower variant into a wider one - lives in "test_msc_variant_types.cpp".
//
// The value the branch binds is checked at runtime in std's "std/variant" and "std/generator" groups, as this suite
// only compiles and verifies.

// The shape the loop rewrite produces for a generator yielding an "Opt".
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariantNarrowing,
  test_valid_sub_variant_pattern_on_a_wider_subject, R"(
    fun f(v: std::option::Opt[S32] or Bool) -> S32 {
        ret case v of {
            is std::option::Opt[S32](..) { 1 }
            else { 0 }
        }
    }
)");

// The members are at different tags in the subject than in the pattern, so the discriminant has to be translated
// rather than copied.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariantNarrowing,
  test_valid_sub_variant_pattern_with_a_different_tag_order, R"(
    cls A { !public a: S32 }
    cls B { !public b: S32 }
    cls C { !public c: S32 }

    sup A ext std::copy::Copy { }
    sup B ext std::copy::Copy { }
    sup C ext std::copy::Copy { }

    type AB = A or B

    fun f(v: C or B or A) -> S32 {
        ret case v of {
            is AB(..) { 1 }
            else { 0 }
        }
    }
)");

// The subject's payload is wider than the narrowed variant's, so only the narrower one's worth of bytes is copied.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariantNarrowing,
  test_valid_sub_variant_pattern_with_a_wider_subject_payload, R"(
    cls Small { !public a: U8 }
    cls Big {
        !public a: U64
        !public b: U64
        !public c: U64
    }
    cls Other { !public a: U8 }

    sup Small ext std::copy::Copy { }
    sup Big ext std::copy::Copy { }
    sup Other ext std::copy::Copy { }

    type SmallOther = Small or Other

    fun f(v: Small or Other or Big) -> S32 {
        ret case v of {
            is SmallOther(..) { 1 }
            else { 0 }
        }
    }
)");

// Every member of the narrowed variant is stateless, so it has no payload to copy at all.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariantNarrowing,
  test_valid_sub_variant_pattern_of_stateless_members, R"(
    cls A { }
    cls B { }
    cls C { !public c: U64 }

    sup A ext std::copy::Copy { }
    sup B ext std::copy::Copy { }
    sup C ext std::copy::Copy { }

    type AB = A or B

    fun f(v: A or B or C) -> S32 {
        ret case v of {
            is AB(..) { 1 }
            else { 0 }
        }
    }
)");

// A borrow is a member in its own right, which is the shape resuming a "Gen[&S32]" gives.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariantNarrowing,
  test_valid_sub_variant_pattern_of_borrowed_members, R"(
    cls A { }

    sup A ext std::copy::Copy { }

    type RefOrA = &S32 or A

    fun f(v: &S32 or A or Bool) -> S32 {
        ret case v of {
            is RefOrA(..) { 1 }
            else { 0 }
        }
    }
)");

// The pattern names a member the subject cannot hold, so it can never match. Matching anything here would be a
// silent always-match, which is what an unmapped narrowing used to do.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestVariantNarrowing,
  test_invalid_sub_variant_pattern_naming_a_member_the_subject_lacks,
  SppTypeMismatchError, R"(
    cls A { }
    cls B { }
    cls C { }

    type AC = A or C

    fun f(v: A or B) -> S32 {
        ret case v of {
            is AC(..) { 1 }
            else { 0 }
        }
    }
)");

// The narrowed value is a variant of its own, so a pattern nested inside it has to be checked against that, not
// against the subject.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariantNarrowing,
  test_valid_sub_variant_pattern_binding_the_narrowed_value, R"(
    fun g(o: std::option::Opt[S32]) -> Void { }

    fun f(v: std::option::Opt[S32] or Bool) -> Void {
        case v of {
            is std::option::Opt[S32](..) { g(v) }
            else { }
        }
    }
)");

// A subject that is not a plain name ("self@" is a dereference) takes the other path through the pattern codegen.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestVariantNarrowing,
  test_valid_sub_variant_pattern_on_a_dereferenced_subject, R"(
    cls Holder { !public v: std::option::Opt[S32] or Bool }

    sup Holder {
        !public
        fun which(&self) -> S32 {
            ret case self.v of {
                is std::option::Opt[S32](..) { 1 }
                else { 0 }
            }
        }
    }
)");
