module spp.analyse.scopes.comp_key;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.comp_time_intrinsics;
import spp.asts.expression_ast;
import spp.utils.ice;
import spp.utils.numbers;
import spp.utils.types;
import genex;
import std;
import numex.big_dec;
import numex.big_int;

namespace spp::analyse::scopes {
  namespace {
    /// Whether a constant named through the "type" (a "TypeId"'s
    /// word) cannot be read yet: its type did not resolve, or
    /// is closed and its constant cannot be reached yet. An open
    /// type's constant is read once a substitution closes it.
    auto MemberUnresolved(
      const std::uint64_t type, const StrView name) -> bool {
      // Check if the type has the unresolved flag set, or its
      // constant can't be reached.
      const auto owner = TypeIdOfWord(type);
      return owner->HasUnresolved
        or (IsClosedTypeId(owner) and utils::comp_generics::FindCompMemberId(owner, name) == nullptr);
    }
  }
}

/// [CHECKED]
auto spp::analyse::scopes::FoldCompValues(
  const StrView op,
  CompKey const &lhs,
  CompKey const &rhs)
  -> Opt<CompKey> {
  namespace ct = utils::comp_time_intrinsics;

  // Two numbers of different types do not mix. An unsuffixed one
  // takes the other side's type, as it does in an expression ("n +
  // 1" with "n: USize"). A shift's count is its own type ("U32"),
  // so a shift is typed by the value shifted alone.
  const auto is_shift = op == "<<" or op == ">>";
  if (not is_shift and not lhs.Text.empty() and not rhs.Text.empty() and lhs.Text != rhs.Text) { return Nullopt; }
  auto typed_lhs = lhs;
  auto typed_rhs = rhs;
  if (typed_lhs.Text.empty() and not is_shift) { typed_lhs.Text = rhs.Text; }
  if (typed_rhs.Text.empty() and not is_shift) { typed_rhs.Text = lhs.Text; }

  // The operation is the intrinsic std's method for the operator
  // calls ("FoldBinaryOp"), so a key folds what a folded call
  // does, to the same value, and leaves unfolded what the call
  // raises, as it does a value no constant of its type can hold.
  const auto l = ct::LiteralOf(typed_lhs);
  const auto r = ct::LiteralOf(typed_rhs);
  if (l == nullptr or r == nullptr) {
    Ice("folding comp values that are neither bool, float nor integer");
  }
  const auto result = ct::FoldBinaryOp(op, *l, *r);
  return result != nullptr ? ct::ValueKeyOf(*result) : Nullopt;
}

/// [CHECKED]
auto spp::analyse::scopes::ParamsNamedBy(
  const CompId id) -> Vec<std::uint64_t> {
  // Collect all the parts of the comp key that are "Param"
  // parts, and push them into one collection.
  auto out = Vec<std::uint64_t>();
  if (id == nullptr) { return out; }
  id->Visit([&out](CompKey const &part) {
    if (part.Kind == CompKey::Part::Param) { out.push_back(part.ParamId); }
  });
  return out;
}

/// [CHECKED]
auto spp::analyse::scopes::MembersNamedBy(
  const CompId id) -> Vec<Pair<std::uint64_t, Str>> {
  // Collect all the parts of the comp key that are "Member"
  // parts, and push them into one collection.
  auto out = Vec<Pair<std::uint64_t, Str>>();
  if (id == nullptr) { return out; }
  id->Visit([&out](CompKey const &part) {
    if (part.Kind == CompKey::Part::Member) { out.emplace_back(part.Type, part.Text); }
  });
  return out;
}

/// [CHECKED]
auto spp::analyse::scopes::IsValueCompId(
  const CompId id) -> bool {
  // If all the parts of a comp id are values or packs, then
  // the comp id can be considered a comp value.
  return id != nullptr and not id->Any([](CompKey const &part) {
    return part.Kind != CompKey::Part::Value and part.Kind != CompKey::Part::Pack;
  });
}

/// [CHECKED]
auto spp::analyse::scopes::U64Of(
  const CompId id) -> Opt<std::uint64_t> {
  // Check the comp id is (internally) holding an integer, and
  // if so, cast it into a U64. Used for array sizes, atomics
  // in the llvm manual lowering, etc.
  const auto value = id != nullptr ? id->AsInt() : nullptr;
  return value != nullptr ? spp::utils::numbers::ToU64(*value) : Nullopt;
}

namespace spp::analyse::scopes {
  namespace {
    /// [CHECKED]
    /// "DoesTypeIdNameAnyGnParams" for a comp value: a comp
    /// parameter of its own ("ParamsNamedBy"), or a parameter of a
    /// type it names a constant through ("Box[T]::n" names "T"),
    /// as a type's "ParamsNamedBy" counts both.
    auto DoesCompIdNameAnyGnParams(
      const CompId id) -> bool {
      // Its own parameters, then those of the types it names
      // constants through, as a type's "ParamsNamedBy"
      // collects both.
      return not ParamsNamedBy(id).empty() or genex::any_of(MembersNamedBy(id), [](auto const &member) {
        return DoesTypeIdNameAnyGnParams(TypeIdOfWord(member.first));
      });
    }
  }
}

/// [CHECKED]
auto spp::analyse::scopes::IsConcreteCompId(
  const CompId id) -> bool {
  // No parameter, and no "Self" in a type it names a constant
  // through, as "IsConcreteTypeId" checks "HasSelf".
  return id != nullptr and not DoesCompIdNameAnyGnParams(id) and not genex::any_of(
    MembersNamedBy(id), [](auto const &member) { return TypeIdOfWord(member.first)->HasSelf; });
}

/// [CHECKED]
auto spp::analyse::scopes::IsReadableCompId(
  const CompId id) -> bool {
  // A readable comp id is a comp id that doesn't have any
  // unresolved parts. It can have generic parts however.
  return id != nullptr and not id->Any([](CompKey const &part) {
    return part.Kind == CompKey::Part::Member and MemberUnresolved(part.Type, part.Text);
  });
}

/// [CHECKED]
auto spp::analyse::scopes::IsClosedCompId(
  const CompId id) -> bool {
  // A closed comp id is a concrete comp id that is also
  // readable.
  return IsConcreteCompId(id) and IsReadableCompId(id);
}

auto spp::analyse::scopes::ClearCompIds() -> void {
  InternedCompKeys().clear();
}
