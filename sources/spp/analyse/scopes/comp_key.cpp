module spp.analyse.scopes.comp_key;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.comp_generics;
import spp.utils.numbers;
import spp.utils.types;
import genex;
import std;
import numex.big_dec;
import numex.big_int;

namespace spp::analyse::scopes {
  namespace {
    /// [CHECKED]
    /// A shift count, as the shift intrinsics read one.
    auto ShiftCount(numex::BigInt const &count) -> std::optional<std::uint32_t> {
      const auto value = spp::utils::numbers::ToU64(count);
      return value.has_value() ? std::optional(static_cast<std::uint32_t>(*value)) : std::nullopt;
    }

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
  -> std::optional<CompKey> {
  // Two bools compare for equality, and nothing else.
  // Todo: "and" & "or" for bools?
  const auto lhs_bool = lhs.AsBool();
  const auto rhs_bool = rhs.AsBool();
  if (lhs_bool != nullptr or rhs_bool != nullptr) {
    if (lhs_bool == nullptr or rhs_bool == nullptr) { return std::nullopt; }
    if (op == "==") { return CompKey::OfBool(*lhs_bool == *rhs_bool); }
    if (op == "!=") { return CompKey::OfBool(*lhs_bool != *rhs_bool); }
    return std::nullopt;
  }

  // Two floats fold exactly, by the four operations and the
  // comparisons; a float never mixes with an integer. The
  // suffixes combine as an integer's do (below).
  const auto lf = lhs.AsFloat();
  const auto rf = rhs.AsFloat();
  if (lf != nullptr or rf != nullptr) {
    if (lf == nullptr or rf == nullptr) { return std::nullopt; }
    if (not lhs.Text.empty() and not rhs.Text.empty() and lhs.Text != rhs.Text) { return std::nullopt; }
    auto const &type = not lhs.Text.empty() ? lhs.Text : rhs.Text;
    const auto of = [&type](numex::BigDec value) { return std::optional(CompKey::OfFloat(std::move(value), type)); };
    if (op == "+") { return of(*lf + *rf); }
    if (op == "-") { return of(*lf - *rf); }
    if (op == "*") { return of(*lf * *rf); }
    if (op == "/") { return *rf == numex::BigDec(0) ? std::nullopt : of(*lf / *rf); }
    if (op == "==") { return CompKey::OfBool(*lf == *rf); }
    if (op == "!=") { return CompKey::OfBool(*lf != *rf); }
    if (op == "<") { return CompKey::OfBool(*lf < *rf); }
    if (op == "<=") { return CompKey::OfBool(*lf <= *rf); }
    if (op == ">") { return CompKey::OfBool(*lf > *rf); }
    if (op == ">=") { return CompKey::OfBool(*lf >= *rf); }
    return std::nullopt;
  }

  // An unsuffixed integer takes the other side's type, as it
  // does in an expression ("n + 1" with "n: USize"); two
  // different types do not mix. A shift's count is its own
  // type ("U32"), so a shift is typed by the value shifted alone.
  const auto l = lhs.AsInt();
  const auto r = rhs.AsInt();
  if (l == nullptr or r == nullptr) { return std::nullopt; }
  auto const &a = *l;
  auto const &b = *r;
  auto const &l_type = lhs.Text;
  auto const &r_type = rhs.Text;
  const auto is_shift = op == "<<" or op == ">>";
  if (not is_shift and not l_type.empty() and not r_type.empty() and l_type != r_type) { return std::nullopt; }
  const auto type = is_shift or not l_type.empty() ? l_type : r_type;
  const auto by_zero = b == numex::BigInt(0);
  const auto of = [&type](numex::BigInt value) { return std::optional(CompKey::OfInt(std::move(value), type)); };

  if (op == "+") { return of(a + b); }
  if (op == "-") { return of(a - b); }
  if (op == "*") { return of(a * b); }
  if (op == "/") { return by_zero ? std::nullopt : of(a / b); }
  if (op == "%") { return by_zero ? std::nullopt : of(a % b); }
  if (op == "|") { return of(a | b); }
  if (op == "&") { return of(a & b); }
  if (op == "^") { return of(a ^ b); }
  if (op == "<<") {
    const auto count = ShiftCount(b);
    auto wrapped = count.has_value() ? spp::utils::numbers::WrapToInteger(a << *count, type) : std::nullopt;
    return wrapped.has_value() ? of(std::move(*wrapped)) : std::nullopt;
  }
  if (op == ">>") {
    const auto count = ShiftCount(b);
    return count.has_value() ? of(a >> *count) : std::nullopt;
  }
  if (op == "==") { return CompKey::OfBool(a == b); }
  if (op == "!=") { return CompKey::OfBool(a != b); }
  if (op == "<") { return CompKey::OfBool(a < b); }
  if (op == "<=") { return CompKey::OfBool(a <= b); }
  if (op == ">") { return CompKey::OfBool(a > b); }
  if (op == ">=") { return CompKey::OfBool(a >= b); }
  return std::nullopt;
}

/// [CHECKED]
auto spp::analyse::scopes::ParamsNamedBy(
  const CompId id) -> std::vector<std::uint64_t> {
  // Collect all the parts of the comp key that are "Param"
  // parts, and push them into one collection.
  auto out = std::vector<std::uint64_t>();
  if (id == nullptr) { return out; }
  id->Visit([&out](CompKey const &part) {
    if (part.Kind == CompKey::Part::Param) { out.push_back(part.ParamId); }
  });
  return out;
}

/// [CHECKED]
auto spp::analyse::scopes::MembersNamedBy(
  const CompId id) -> std::vector<std::pair<std::uint64_t, Str>> {
  // Collect all the parts of the comp key that are "Member"
  // parts, and push them into one collection.
  auto out = std::vector<std::pair<std::uint64_t, Str>>();
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
  const CompId id) -> std::optional<std::uint64_t> {
  // Check the comp id is (internally) holding an integer, and
  // if so, cast it into a U64. Used for array sizes, atomics
  // in the llvm manual lowering, etc.
  const auto value = id != nullptr ? id->AsInt() : nullptr;
  return value != nullptr ? spp::utils::numbers::ToU64(*value) : std::nullopt;
}

/// [CHECKED]
auto spp::analyse::scopes::DoesCompIdNameAnyGnParams(
  const CompId id) -> bool {
  // Its own parameters, then those of the types it names
  // constants through, as a type's "ParamsNamedBy"
  // collects both.
  return not ParamsNamedBy(id).empty() or genex::any_of(MembersNamedBy(id), [](auto const &member) {
    return DoesTypeIdNameAnyGnParams(TypeIdOfWord(member.first));
  });
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
