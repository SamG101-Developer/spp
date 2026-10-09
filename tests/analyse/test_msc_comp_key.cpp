#include "../test_macros.hpp"

import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.type_key;
import numex.big_dec;
import numex.big_int;

// A comp value's identity is a tree ("scopes::CompKey"), interned as it is ("InternCompKey"), and read, rewritten and
// folded without an ast ("scopes::comp_key"): an operation over a comp parameter is substituted and folded at any depth,
// as a type's arguments are.

namespace {
  using spp::analyse::scopes::CompId;
  using spp::analyse::scopes::CompKey;
  using spp::analyse::scopes::FoldCompValues;
  using spp::analyse::scopes::GenericSubst;
  using spp::analyse::scopes::InternCompKey;
  using spp::analyse::scopes::SubstituteCompId;
  using Kind = CompKey::Part;
  using Node = std::optional<CompKey>;

  auto Int(const std::int64_t value, std::string const &suffix) -> CompKey {
    return CompKey::OfInt(numex::BigInt(value), suffix);
  }

  auto Bool(const bool value) -> CompKey { return CompKey::OfBool(value); }

  /// A float value "num / den" (an exact rational), of the type "suffix" spells.
  auto Float(const std::int64_t num, const std::int64_t den, std::string const &suffix) -> CompKey {
    return CompKey::OfFloat(numex::BigDec(numex::BigInt(num), numex::BigInt(den)), suffix);
  }

  auto Param(const std::uint64_t id) -> CompKey { return CompKey::OfParam(id); }

  /// A pack and an operation, their parts interned as a node's are.
  auto Pack(std::vector<CompKey> const &elems) -> CompKey {
    auto kids = std::vector<CompId>();
    for (auto const &elem : elems) { kids.push_back(InternCompKey(CompKey(elem))); }
    return CompKey::OfPack(std::move(kids));
  }

  auto Op(CompKey const &lhs, std::string const &op, CompKey const &rhs) -> CompKey {
    return CompKey::OfOp(op, InternCompKey(CompKey(lhs)), InternCompKey(CompKey(rhs)));
  }

  /// A substitution binding each comp parameter in "bound" ("CompParams"), the ones in "packs" as packs.
  auto Subst(spp::Map<std::uint64_t, CompKey> const &bound, std::vector<std::uint64_t> packs = {}) -> GenericSubst {
    auto subst = GenericSubst();
    for (auto const &[id, value] : bound) { subst.CompParams.emplace_back(id, InternCompKey(CompKey(value))); }
    subst.CompPackParams = std::move(packs);
    return subst;
  }

  /// "node"'s identity ("InternCompKey").
  auto Id(CompKey const &node) -> CompId { return InternCompKey(CompKey(node)); }

  /// "node"'s identity with each parameter in "bound" replaced ("SubstituteCompId"), those in "packs" spread.
  auto Rewrite(
    CompKey const &node, spp::Map<std::uint64_t, CompKey> const &bound, std::vector<std::uint64_t> packs = {})
    -> CompId {
    return SubstituteCompId(Id(node), Subst(bound, std::move(packs)));
  }
}

TEST(TestCompKey, test_interning_is_by_structure) {
  // The same tree, built twice, is one identity.
  EXPECT_EQ(InternCompKey(Op(Param(1), "+", Int(1, "uz"))), InternCompKey(Op(Param(1), "+", Int(1, "uz"))));
  EXPECT_EQ(InternCompKey(Pack({})), InternCompKey(Pack({})));

  // The parts are interned, so a tree built twice shares its parts: equality reads one pointer per part.
  EXPECT_EQ(Op(Param(1), "+", Int(1, "uz")).Kids, Op(Param(1), "+", Int(1, "uz")).Kids);

  // A value and a parameter of the same number are different parts, not different spellings.
  EXPECT_NE(InternCompKey(Int(5, "")), InternCompKey(Param(5)));

  // A value is its number and its type: the same number of another type, or unsuffixed, is another value, and a bool is
  // never an integer.
  EXPECT_EQ(InternCompKey(Int(5, "uz")), InternCompKey(Int(5, "uz")));
  EXPECT_NE(InternCompKey(Int(5, "uz")), InternCompKey(Int(5, "u8")));
  EXPECT_NE(InternCompKey(Int(5, "uz")), InternCompKey(Int(5, "")));
  EXPECT_NE(InternCompKey(Int(1, "")), InternCompKey(Bool(true)));
  EXPECT_NE(InternCompKey(Bool(true)), InternCompKey(Bool(false)));

  // An operator has fixed arity, so the grouping is the shape of the tree.
  const auto a = Param(1);
  const auto b = Param(2);
  const auto c = Param(3);
  EXPECT_NE(InternCompKey(Op(Op(a, "+", b), "*", c)), InternCompKey(Op(a, "+", Op(b, "*", c))));

  // Members and opaque spellings are part of the tree too.
  EXPECT_NE(InternCompKey(CompKey::OfMember(7, "n")), InternCompKey(CompKey::OfMember(8, "n")));
  EXPECT_NE(InternCompKey(CompKey::OfMember(7, "n")), InternCompKey(CompKey::OfMember(7, "m")));
}

TEST(TestCompKey, test_fold_values) {
  EXPECT_EQ(FoldCompValues("+", Int(2, "uz"), Int(3, "uz")), Int(5, "uz"));
  EXPECT_EQ(FoldCompValues("-", Int(2, "s32"), Int(3, "s32")), Int(-1, "s32"));
  EXPECT_EQ(FoldCompValues("*", Int(2, ""), Int(3, "uz")), Int(6, "uz"));
  EXPECT_EQ(FoldCompValues("+", Int(2, "u8"), Int(3, "uz")), std::nullopt);
  EXPECT_EQ(FoldCompValues("/", Int(2, "uz"), Int(0, "uz")), std::nullopt);
  EXPECT_EQ(FoldCompValues("%", Int(7, "uz"), Int(4, "uz")), Int(3, "uz"));
  EXPECT_EQ(FoldCompValues("<<", Int(1, "u8"), Int(8, "u8")), Int(0, "u8"));
  EXPECT_EQ(FoldCompValues("<<", Int(3, "uz"), Int(2, "u32")), Int(12, "uz"));
  EXPECT_EQ(FoldCompValues(">>", Int(8, "uz"), Int(2, "uz")), Int(2, "uz"));
  EXPECT_EQ(FoldCompValues("<", Int(1, "uz"), Int(2, "uz")), Bool(true));
  EXPECT_EQ(FoldCompValues("==", Bool(true), Bool(false)), Bool(false));
  EXPECT_EQ(FoldCompValues("+", Bool(true), Bool(false)), std::nullopt);
  EXPECT_EQ(FoldCompValues("==", Bool(true), Int(1, "")), std::nullopt);

  // Past 64 bits, as a "BigInt" holds it.
  const auto big = numex::BigInt("18446744073709551616");
  EXPECT_EQ(
    FoldCompValues("*", CompKey::OfInt(big, "u128"), Int(2, "u128")),
    CompKey::OfInt(numex::BigInt("36893488147419103232"), "u128"));
}

TEST(TestCompKey, test_float_values_are_exact_rationals) {
  // A float is its exact value, in lowest terms: however it is spelled or reached, one value is one identity.
  EXPECT_EQ(InternCompKey(CompKey::OfFloat(numex::BigDec("1.5"), "f64")),
    InternCompKey(CompKey::OfFloat(numex::BigDec("1.50"), "f64")));
  EXPECT_EQ(InternCompKey(Float(1, 3, "f64")), InternCompKey(Float(2, 6, "f64")));
  EXPECT_EQ(InternCompKey(Float(3, 2, "f64")), InternCompKey(CompKey::OfFloat(numex::BigDec("1.5"), "f64")));

  // Its type is part of it, and a float is never an integer of the same number.
  EXPECT_NE(InternCompKey(Float(3, 2, "f64")), InternCompKey(Float(3, 2, "f32")));
  EXPECT_NE(InternCompKey(Float(2, 1, "")), InternCompKey(Int(2, "")));
  EXPECT_NE(InternCompKey(Float(1, 3, "f64")), InternCompKey(Float(1, 4, "f64")));
}

TEST(TestCompKey, test_fold_float_values) {
  // The four operations fold exactly: a third stays a third, and three of them are one.
  EXPECT_EQ(FoldCompValues("/", Float(1, 1, "f64"), Float(3, 1, "f64")), Float(1, 3, "f64"));
  EXPECT_EQ(FoldCompValues("/", Float(2, 1, "f64"), Float(6, 1, "f64")), Float(1, 3, "f64"));
  EXPECT_EQ(FoldCompValues("*", Float(1, 3, "f64"), Float(3, 1, "f64")), Float(1, 1, "f64"));
  EXPECT_EQ(FoldCompValues("+", Float(1, 10, "f64"), Float(2, 10, "f64")), Float(3, 10, "f64"));
  EXPECT_EQ(FoldCompValues("-", Float(1, 2, "f32"), Float(3, 4, "f32")), Float(-1, 4, "f32"));
  EXPECT_EQ(FoldCompValues("/", Float(1, 1, "f64"), Float(0, 1, "f64")), std::nullopt);

  // An unsuffixed side takes the other side's type; two types do not mix, nor do a float and an integer.
  EXPECT_EQ(FoldCompValues("*", Float(3, 2, "f64"), Float(2, 1, "")), Float(3, 1, "f64"));
  EXPECT_EQ(FoldCompValues("+", Float(1, 1, "f32"), Float(1, 1, "f64")), std::nullopt);
  EXPECT_EQ(FoldCompValues("+", Float(1, 1, "f64"), Int(1, "")), std::nullopt);
  EXPECT_EQ(FoldCompValues("%", Float(7, 1, "f64"), Float(2, 1, "f64")), std::nullopt);

  // Comparisons fold to bools, on the exact values.
  EXPECT_EQ(FoldCompValues("<", Float(1, 3, "f64"), Float(34, 100, "f64")), Bool(true));
  EXPECT_EQ(FoldCompValues("==", Float(1, 3, "f64"), Float(2, 6, "f64")), Bool(true));
  EXPECT_EQ(FoldCompValues("!=", Float(1, 3, "f64"), Float(333, 1000, "f64")), Bool(true));
}

TEST(TestCompKey, test_rewrite_folds_a_float_operation) {
  // "x * 2.0" with "x" bound to "1.5" is "3.0"; "x / 3.0" with "x" bound to "1.0" is a third.
  EXPECT_EQ(Rewrite(Op(Param(1), "*", Float(2, 1, "f64")), {{1, Float(3, 2, "f64")}}), Id(Float(3, 1, "f64")));
  EXPECT_EQ(Rewrite(Op(Param(1), "/", Float(3, 1, "f64")), {{1, Float(1, 1, "f64")}}), Id(Float(1, 3, "f64")));
}

TEST(TestCompKey, test_rewrite_inside_an_operation) {
  EXPECT_EQ(Rewrite(Op(Op(Param(1), "+", Int(1, "uz")), "*", Int(2, "uz")), {{1, Int(3, "uz")}}), Id(Int(8, "uz")));
  EXPECT_EQ(Rewrite(Op(Param(1), "+", Param(2)), {{1, Int(3, "uz")}}), Id(Op(Int(3, "uz"), "+", Param(2))));
  EXPECT_EQ(
    Rewrite(Pack({Param(1), Op(Param(1), "+", Int(1, "uz"))}), {{1, Int(1, "uz")}}),
    Id(Pack({Int(1, "uz"), Int(2, "uz")})));
}

TEST(TestCompKey, test_rewrite_spreads_a_pack) {
  EXPECT_EQ(
    Rewrite(Pack({Int(0, "uz"), Param(7)}), {{7, Pack({Int(1, "uz"), Int(2, "uz")})}}, {7}),
    Id(Pack({Int(0, "uz"), Int(1, "uz"), Int(2, "uz")})));
}

// An operation over a comp parameter, nested, in a return type: each instantiation reads it with its own binding.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCompKey,
    test_valid_nested_operation_in_a_return_type, R"(
    cls A[cmp n: USize] { }
    fun f[cmp n: USize]() -> A[(n + 1_uz) * 2_uz] { ret A[(n + 1_uz) * 2_uz]() }
    fun g() -> Void {
        let x: A[4_uz] = f[1_uz]()
        let y: A[6_uz] = f[2_uz]()
        std::mem::ops::drop(x)
        std::mem::ops::drop(y)
    }
)");

// A comp default that is an operation over an earlier parameter is read with that parameter bound.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCompKey,
    test_valid_operation_comp_default, R"(
    cls A[cmp n: USize] { }
    fun f[cmp n: USize, cmp m: USize = n * 2_uz]() -> A[m] { ret A[m]() }
    fun g() -> Void {
        let x: A[6_uz] = f[3_uz]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCompKey,
    test_valid_shift_in_a_return_type, R"(
    cls A[cmp n: USize] { }
    fun f[cmp n: USize]() -> A[n << 2_u32] { ret A[n << 2_u32]() }
    fun g() -> Void {
        let x: A[12_uz] = f[3_uz]()
        std::mem::ops::drop(x)
    }
)");

// A float comp argument is its exact value: spellings of one value are one type.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCompKey,
    test_valid_float_spellings_are_one_type, R"(
    cls A[cmp x: F64] { }
    fun g() -> Void {
        let a: A[1.5_f64] = A[1.50_f64]()
        let b: A[0.75_f64] = A[0.5_f64 + 0.25_f64]()
        std::mem::ops::drop(a)
        std::mem::ops::drop(b)
    }
)");

// A value with no finite decimal is still one value: a third, however it is reached.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCompKey,
    test_valid_float_fraction_is_one_type, R"(
    cls A[cmp x: F64] { }
    fun g() -> Void {
        let a: A[1.0_f64 / 3.0_f64] = A[2.0_f64 / 6.0_f64]()
        std::mem::ops::drop(a)
    }
)");

// An operation over a float comp parameter folds with the parameter bound.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCompKey,
    test_valid_float_operation_over_a_parameter, R"(
    cls A[cmp x: F64] { }
    fun f[cmp x: F64]() -> A[x * 2.0_f64] { ret A[x * 2.0_f64]() }
    fun g() -> Void {
        let a: A[3.0_f64] = f[1.5_f64]()
        std::mem::ops::drop(a)
    }
)");

// A third reached through a substitution is the written third: the instance is rebuilt as "1.0 / 3.0", which folds
// back to the same value, where a decimal cut short would be another type.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCompKey,
    test_valid_float_fraction_through_a_substitution, R"(
    cls A[cmp x: F64] { }
    fun f[cmp x: F64]() -> A[x / 3.0_f64] { ret A[x / 3.0_f64]() }
    fun g() -> Void {
        let a: A[2.0_f64 / 6.0_f64] = f[1.0_f64]()
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestCompKey,
    test_invalid_different_float_values_are_different_types,
    SppTypeMismatchError, R"(
    cls A[cmp x: F64] { }
    fun g() -> Void {
        let a: A[1.0_f64 / 3.0_f64] = A[0.333_f64]()
        std::mem::ops::drop(a)
    }
)");

// A comparison folds to a bool comp argument.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCompKey,
    test_valid_comparison_folds_to_a_bool, R"(
    cls C[cmp b: Bool] { }
    fun f[cmp n: USize]() -> C[n == 1_uz] { ret C[n == 1_uz]() }
    fun g() -> Void {
        let x: C[true] = f[1_uz]()
        let y: C[false] = f[2_uz]()
        std::mem::ops::drop(x)
        std::mem::ops::drop(y)
    }
)");

// A pack element that is an operation over a parameter is folded with the rest of the pack.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCompKey,
    test_valid_operation_as_a_pack_element, R"(
    cls P[cmp ..ns: USize] { }
    fun f[cmp n: USize]() -> P[n, n + 1_uz] { ret P[n, n + 1_uz]() }
    fun g() -> Void {
        let x: P[1_uz, 2_uz] = f[1_uz]()
        std::mem::ops::drop(x)
    }
)");

// A class's comp default that is an operation is keyed by the value it folds to, so the defaulted and the written
// spellings are one type.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCompKey,
    test_valid_class_operation_comp_default_keys_as_its_value, R"(
    cls B[cmp n: USize, cmp m: USize = n + 1_uz] { }
    fun g() -> Void {
        let b: B[1_uz] = B[1_uz, 2_uz]()
        std::mem::ops::drop(b)
    }
)");

// A constant named through "Self" in a type's comp argument is the constant of the type it was written in, read
// wherever the type is used: here through a receiver, from a block whose own "Self::n" is another value.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCompKey,
    test_valid_self_member_in_a_return_type_read_through_a_receiver, R"(
    cls A[cmp n: USize] { }
    cls Buf { }
    sup Buf {
        !public cmp n: USize = 4_uz
        !public fun make(&self) -> A[Self::n] { ret A[Self::n]() }
    }
    cls Other { }
    sup Other {
        !public cmp n: USize = 8_uz
        !public fun read(&self, b: &Buf) -> Void {
            let x: A[4_uz] = b.make()
            std::mem::ops::drop(x)
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestCompKey,
    test_invalid_self_member_in_a_return_type_is_not_the_callers,
    SppTypeMismatchError, R"(
    cls A[cmp n: USize] { }
    cls Buf { }
    sup Buf {
        !public cmp n: USize = 4_uz
        !public fun make(&self) -> A[Self::n] { ret A[Self::n]() }
    }
    cls Other { }
    sup Other {
        !public cmp n: USize = 8_uz
        !public fun read(&self, b: &Buf) -> Void {
            let x: A[8_uz] = b.make()
            std::mem::ops::drop(x)
        }
    }
)");

// The same through a generic owner: each instantiation reads its own block's constant.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCompKey,
    test_valid_self_member_in_a_return_type_on_a_generic_owner, R"(
    cls A[cmp n: USize] { }
    cls Box[T] { }
    sup [T] Box[T] {
        !public cmp n: USize = 2_uz
        !public fun make(&self) -> A[Self::n] { ret A[Self::n]() }
    }
    fun f(b: &Box[S32]) -> Void {
        let x: A[2_uz] = b.make()
        std::mem::ops::drop(x)
    }
)");

// A method's mock is typed from outside its block (stage 1), so "Self" in a parameter's comp argument is its owner.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCompKey,
    test_valid_self_member_in_a_method_value_parameter, R"(
    cls A[cmp n: USize] { }
    cls Buf { }
    sup Buf {
        !public cmp n: USize = 4_uz
        !public fun take(&self, a: A[Self::n]) -> Void { std::mem::ops::drop(a) }
    }
    fun apply(f: std::function::FunRef[(&Buf, A[4_uz]), Void], b: &Buf) -> Void { f(b, A[4_uz]()) }
    fun g(b: Buf) -> Void {
        apply(Buf::take, &b)
        std::mem::ops::drop(b)
    }
)");

// A "sup" block's type alias naming "Self::n", used from outside the block.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCompKey,
    test_valid_self_member_in_a_sup_type_alias, R"(
    cls A[cmp n: USize] { }
    cls Buf { }
    sup Buf {
        !public cmp n: USize = 4_uz
        !public type Four = A[Self::n]
    }
    fun g() -> Void {
        let x: Buf::Four = A[4_uz]()
        std::mem::ops::drop(x)
    }
)");

// A default naming "Self::n", carried to a call made from a block whose own "Self::n" is another value.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCompKey,
    test_valid_self_member_in_a_default_read_at_the_call, R"(
    cls A[cmp n: USize] { }
    cls Buf { }
    sup Buf {
        !public cmp n: USize = 4_uz
        !public fun take(&self, a: A[Self::n] = A[Self::n]()) -> A[Self::n] { ret a }
    }
    cls Other { }
    sup Other {
        !public cmp n: USize = 8_uz
        !public fun read(&self, b: &Buf) -> Void {
            let x: A[4_uz] = b.take()
            std::mem::ops::drop(x)
        }
    }
)");

// A comp default named through "Self" on a generic owner, by identity.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCompKey,
    test_valid_self_member_comp_default_on_a_generic_owner, R"(
    cls A[cmp n: USize] { }
    cls Box[T] { }
    sup [T] Box[T] {
        !public cmp k: USize = 3_uz
        !public fun pick[cmp m: USize = Self::k](&self) -> A[m] { ret A[m]() }
    }
    fun f(b: &Box[S32]) -> Void {
        let x: A[3_uz] = b.pick()
        std::mem::ops::drop(x)
    }
)");

// A constant whose value names its generic block's parameter ("k + 1_uz") has no value of its own: it is read where an
// instantiation binds the parameter, each instantiation its own value.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCompKey,
    test_valid_self_member_depending_on_the_owners_parameter, R"(
    cls A[cmp n: USize] { }
    cls Box[cmp k: USize] { }
    sup [cmp k: USize] Box[k] {
        !public cmp n: USize = k + 1_uz
        !public fun make(&self) -> A[Self::n] { ret A[Self::n]() }
    }
    fun f(b: &Box[3_uz], c: &Box[5_uz]) -> Void {
        let x: A[4_uz] = b.make()
        let y: A[6_uz] = c.make()
        std::mem::ops::drop(x)
        std::mem::ops::drop(y)
    }
)");

// The same constant named directly through each instantiation, and borrowed (a comparison takes its operands by
// borrow, and a folded constant has no storage of its own).
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCompKey,
    test_valid_generic_sup_constant_naming_the_blocks_parameter, R"(
    cls A[cmp n: USize] { }
    cls Box[cmp k: USize] { }
    sup [cmp k: USize] Box[k] {
        !public cmp n: USize = k + 1_uz
    }
    fun f() -> Bool {
        let x: A[4_uz] = A[Box[3_uz]::n]()
        std::mem::ops::drop(x)
        ret Box[3_uz]::n == 4_uz and Box[5_uz]::n == 6_uz
    }
)");
