#include "test_macros.hpp"

import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.asts.type_identifier_ast;

// A variant's members are put in one order ("scopes::StableKeyLess"), which its tags follow. The order reads a symbol's
// name and the order it was made in, never its address, so the tags do not change with where things were allocated.

namespace {
  using spp::analyse::scopes::TypeKey;
  using spp::analyse::scopes::InternTypeKey;
  using spp::analyse::scopes::StableKeyLess;
  using spp::analyse::scopes::TypeKind;
  using spp::analyse::scopes::TypeSymbol;

  /// A class symbol named "name". Never freed: an interned key holds its address for the rest of the process.
  auto Cls(spp::Str const &name) -> TypeSymbol const& {
    return *new TypeSymbol(spp::asts::TypeIdentifierAst::FromString(name), nullptr, nullptr, nullptr, TypeKind::Cls);
  }

  auto KeyOf(TypeSymbol const &sym) -> TypeKey {
    auto key = TypeKey();
    key.PushSymbol(&sym);
    return key;
  }

  /// A key holding "inner" as a nested type, as a variant member's generic argument does.
  auto Wrapping(TypeSymbol const &outer, TypeSymbol const &inner) -> TypeKey {
    auto key = KeyOf(outer);
    key.PushTypeId(InternTypeKey(KeyOf(inner)));
    return key;
  }

  /// Exactly one of the two is before the other.
  auto Ordered(TypeKey const &before, TypeKey const &after) -> bool {
    return StableKeyLess(before, after) and not StableKeyLess(after, before);
  }
}

TEST(TestVariantMemberOrder, test_order_is_by_name_whichever_is_made_first) {
  // Made in both orders, so one of the two pairs has its addresses the other way round from its names.
  auto const &zeta_first = Cls("Zeta");
  auto const &alpha_second = Cls("Alpha");
  auto const &alpha_first = Cls("Alpha");
  auto const &zeta_second = Cls("Zeta");
  EXPECT_TRUE(Ordered(KeyOf(alpha_second), KeyOf(zeta_first)));
  EXPECT_TRUE(Ordered(KeyOf(alpha_first), KeyOf(zeta_second)));
}

TEST(TestVariantMemberOrder, test_same_name_is_by_the_order_made) {
  // Two types named alike in different modules ("io::Error", "fs::Error") are told apart by which was made first.
  auto const &first = Cls("Error");
  auto const &second = Cls("Error");
  EXPECT_TRUE(Ordered(KeyOf(first), KeyOf(second)));
}

TEST(TestVariantMemberOrder, test_nested_types_are_read_by_the_same_order) {
  // "Box[Alpha]" before "Box[Zeta]", whichever argument was made first.
  auto const &box = Cls("Box");
  auto const &zeta = Cls("Zeta");
  auto const &alpha = Cls("Alpha");
  EXPECT_TRUE(Ordered(Wrapping(box, alpha), Wrapping(box, zeta)));
}

TEST(TestVariantMemberOrder, test_order_is_total_and_irreflexive) {
  auto const &a = Cls("Same");
  auto const &b = Cls("Same");
  const auto ka = KeyOf(a);
  const auto kb = KeyOf(b);
  EXPECT_FALSE(StableKeyLess(ka, ka));
  EXPECT_NE(StableKeyLess(ka, kb), StableKeyLess(kb, ka));
}

// Written in any order, one variant: one type, so one set of tags (the value passes from one spelling to the other
// without being rebuilt), including through a generic argument and a substitution.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantMemberOrder,
    test_valid_every_written_order_is_one_type, R"(
    cls Alpha { }
    cls Zeta { }
    fun f(x: Zeta or Alpha or Bool) -> Bool or Alpha or Zeta { ret x }
    fun g(x: Alpha or Bool or Zeta) -> Alpha or Zeta or Bool { ret f(x) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantMemberOrder,
    test_valid_substituted_variant_is_the_written_one, R"(
    cls Alpha { }
    cls Zeta { }
    fun id[T](x: T) -> T { ret x }
    fun g(x: Zeta or Alpha) -> Alpha or Zeta {
        let y: Alpha or Zeta = id[Zeta or Alpha](x)
        ret y
    }
)");

// Members differing only by a float or bool comp argument are ordered by value, so either written order is one type.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantMemberOrder,
    test_valid_members_differing_by_a_float_or_bool_comp_argument, R"(
    cls A[cmp x: F64] { }
    cls C[cmp b: Bool] { }
    fun f(v: A[2.5_f64] or A[1.5_f64]) -> Void { std::mem::ops::drop(v) }
    fun h(v: C[true] or C[false]) -> Void { std::mem::ops::drop(v) }
    fun g() -> Void {
        let a: A[1.5_f64] or A[2.5_f64] = A[1.5_f64]()
        f(a)
        let c: C[false] or C[true] = C[true]()
        h(c)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantMemberOrder,
    test_valid_members_differing_by_a_comp_operation, R"(
    cls A[cmp n: USize] { }
    fun f[cmp n: USize](v: A[n + 2_uz] or A[n + 1_uz]) -> Void { std::mem::ops::drop(v) }
    fun g() -> Void {
        let a: A[2_uz] or A[3_uz] = A[2_uz]()
        f[1_uz](a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestVariantMemberOrder,
    test_invalid_members_differing_by_a_comp_operation_wrong_binding,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A[cmp n: USize] { }
    fun f[cmp n: USize](v: A[n + 2_uz] or A[n + 1_uz]) -> Void { std::mem::ops::drop(v) }
    fun g() -> Void {
        let a: A[2_uz] or A[3_uz] = A[2_uz]()
        f[2_uz](a)
    }
)");
