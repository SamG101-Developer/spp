#include "../test_macros.hpp"

import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.type_key;
import numex.big_int;

// A type part that does not resolve is held inline in its parent key (its words, after its length) rather than by its
// id, and decoding it builds its key again from those words ("ArgsOf", "HeadOf"). The key built again carries the flags
// the part had when it was pushed: here, a "Self" it holds only through a part of its own, by id.

namespace {
  using spp::analyse::scopes::ArgsOf;
  using spp::analyse::scopes::FindArgOf;
  using spp::analyse::scopes::CompKey;
  using spp::analyse::scopes::InternCompKey;
  using spp::analyse::scopes::InternTypeKey;
  using spp::analyse::scopes::TypeId;
  using spp::analyse::scopes::TypeIdWord;
  using spp::analyse::scopes::TypeKey;
  using Tag = TypeKey::Tag;

  /// "Self", interned: parameter 0, a word of its own.
  auto SelfType() -> TypeId {
    auto key = TypeKey();
    key.Push(Tag::TypeParam, 0);
    return InternTypeKey(std::move(key));
  }

  /// An arguments key holding "part" as its one positional argument, interned, and that argument decoded back.
  auto RoundTrip(TypeKey &&part) -> TypeId {
    auto args = TypeKey();
    args.Push(Tag::Pos, 0);
    args.PushTypePart(InternTypeKey(std::move(part)));
    const auto decoded = ArgsOf(InternTypeKey(std::move(args)));
    return decoded.size() == 1 ? decoded[0].TypeVal : nullptr;
  }

  /// A type of its own, interned: distinct words for each distinct text.
  auto NamedType(const std::string_view text) -> TypeId {
    auto key = TypeKey();
    key.PushText(Tag::Name, text);
    return InternTypeKey(std::move(key));
  }

  /// Parameter identities for the keys below: arbitrary, distinct, and never handed out to a real parameter.
  constexpr auto kParamT = static_cast<std::uint64_t>(0xF00001);
  constexpr auto kParamN = static_cast<std::uint64_t>(0xF00002);

  /// "[Pos: A][T=B][Pos: C][n=4]": arguments for parameters (type and comp) between positional ones. The positions
  /// are given as the parameters' identities, so a lookup that read a position word as a parameter would find them.
  auto MixedArgs() -> TypeId {
    auto args = TypeKey();
    args.Push(Tag::Pos, kParamT);
    args.PushTypePart(NamedType("test_find_arg_of_a"));
    args.Push(Tag::Arg, kParamT);
    args.PushTypePart(NamedType("test_find_arg_of_b"));
    args.Push(Tag::Pos, kParamN);
    args.PushTypePart(NamedType("test_find_arg_of_c"));
    args.Push(Tag::Arg, kParamN);
    args.PushCompPart(InternCompKey(CompKey::OfInt(numex::BigInt(4), "USize")));
    return InternTypeKey(std::move(args));
  }
}

TEST(TestTypeKey, test_inline_part_keeps_a_self_held_by_id) {
  // Unresolved, so held inline; its "Self" is only in a part it holds by id.
  auto part = TypeKey();
  part.PushText(Tag::Unresolved, "test_inline_part_keeps_a_self_held_by_id");
  part.PushTypeId(SelfType());
  ASSERT_TRUE(part.HasSelf and part.HasUnresolved);
  const auto decoded = RoundTrip(std::move(part));
  ASSERT_NE(decoded, nullptr);
  EXPECT_TRUE(decoded->HasUnresolved);
  EXPECT_TRUE(decoded->HasSelf);
}

TEST(TestTypeKey, test_inline_part_keeps_a_self_named_through_a_constant) {
  // Its "Self" is only the type a comp part names a constant through ("Self::n").
  auto part = TypeKey();
  part.PushText(Tag::Unresolved, "test_inline_part_keeps_a_self_named_through_a_constant");
  part.PushCompPart(InternCompKey(CompKey::OfMember(TypeIdWord(SelfType()), "n")));
  ASSERT_TRUE(part.HasSelf and part.HasUnresolved);
  const auto decoded = RoundTrip(std::move(part));
  ASSERT_NE(decoded, nullptr);
  EXPECT_TRUE(decoded->HasSelf);
}

TEST(TestTypeKey, test_inline_part_without_self_has_none) {
  auto part = TypeKey();
  part.PushText(Tag::Unresolved, "test_inline_part_without_self_has_none");
  const auto decoded = RoundTrip(std::move(part));
  ASSERT_NE(decoded, nullptr);
  EXPECT_FALSE(decoded->HasSelf);
}

// "FindArgOf" reads the arguments in place: only an argument for a parameter matches, and its kind is as found.

TEST(TestTypeKey, test_find_arg_of_finds_a_type_argument) {
  const auto arg = FindArgOf(MixedArgs(), kParamT);
  ASSERT_TRUE(arg.has_value());
  EXPECT_TRUE(arg->Named);
  EXPECT_FALSE(arg->Spelled);
  EXPECT_EQ(arg->TypeVal, NamedType("test_find_arg_of_b"));
  EXPECT_EQ(arg->CompVal, nullptr);
}

TEST(TestTypeKey, test_find_arg_of_finds_a_comp_argument) {
  const auto arg = FindArgOf(MixedArgs(), kParamN);
  ASSERT_TRUE(arg.has_value());
  EXPECT_EQ(arg->CompVal, InternCompKey(CompKey::OfInt(numex::BigInt(4), "USize")));
  EXPECT_EQ(arg->TypeVal, nullptr);
}

TEST(TestTypeKey, test_find_arg_of_skips_positional_arguments) {
  // Only positional arguments, at the positions the parameters' identities are: neither is for a parameter.
  auto args = TypeKey();
  args.Push(Tag::Pos, kParamT);
  args.PushTypePart(NamedType("test_find_arg_of_a"));
  args.Push(Tag::Pos, kParamN);
  args.PushTypePart(NamedType("test_find_arg_of_c"));
  const auto id = InternTypeKey(std::move(args));
  EXPECT_FALSE(FindArgOf(id, kParamT).has_value());
  EXPECT_FALSE(FindArgOf(id, kParamN).has_value());
  EXPECT_EQ(ArgsOf(id).size(), 2uz);
}

TEST(TestTypeKey, test_find_arg_of_skips_a_spelled_argument) {
  // A keyword naming no parameter is spelled; its interned name is no parameter's identity, even when equal to one.
  auto args = TypeKey();
  args.Push(Tag::Name, kParamT);
  args.PushTypePart(NamedType("test_find_arg_of_a"));
  const auto id = InternTypeKey(std::move(args));
  EXPECT_FALSE(FindArgOf(id, kParamT).has_value());
  ASSERT_EQ(ArgsOf(id).size(), 1uz);
  EXPECT_TRUE(ArgsOf(id)[0].Named and ArgsOf(id)[0].Spelled);
}

TEST(TestTypeKey, test_find_arg_of_misses_an_absent_parameter) {
  EXPECT_FALSE(FindArgOf(MixedArgs(), 0xF0000F).has_value());
  EXPECT_FALSE(FindArgOf(nullptr, kParamT).has_value());
}

TEST(TestTypeKey, test_find_arg_of_agrees_with_args_of) {
  // Every argument for a parameter "ArgsOf" decodes is the one "FindArgOf" finds by that parameter.
  for (auto const &arg : ArgsOf(MixedArgs())) {
    if (not arg.Named) { continue; }
    const auto found = FindArgOf(MixedArgs(), arg.Slot);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->TypeVal, arg.TypeVal);
    EXPECT_EQ(found->CompVal, arg.CompVal);
  }
  EXPECT_EQ(ArgsOf(MixedArgs()).size(), 4uz);
}
