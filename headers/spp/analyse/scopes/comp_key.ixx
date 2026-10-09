module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.comp_key;
import spp.utils.types;
import genex;
import std;
import numex.big_dec;
import numex.big_int;

namespace spp::analyse::scopes {
  /// The "comp key" is an identity that a comp expression is
  /// tagged as. It holds all the internal expressions, performs
  /// folding where possible etc, in order to unify A[1 + 2] and
  /// A[3] etc. Uses a tree structure rather than the type-like
  /// flat vector, for folding purposes.
  SPP_EXP_CLS struct CompKey {
    /// A large set of possible types of a comp, which might
    /// require certain behaviour and handling.
    enum class Part : std::uint8_t {
      Value, Param, Pack, Op, Opaque, Member
    };

    /// The kind is on the node because the node takes part
    /// in the tree itself, rather than holding a vector of
    /// nodes.
    Part Kind = Part::Value;

    /// "Value": an [integer/float]'s type suffix; "Op": the
    /// operator ("+"); "Opaque": the spelling; "Member": the
    /// constant's name
    Str Text;

    /// "Param": the comp parameter's "ParamId"
    std::uint64_t ParamId = 0;

    /// "Pack": the elements; "Op": the two operands. Each is
    /// interned with "InternCompKey", as a type key's nested
    /// types are, so a node's equality and hash read one
    /// pointer per part rather than walking the parts.
    std::vector<CompKey const*> Kids;

    /// "Member": the type the constant is named through, as
    /// its "TypeId"'s word.
    std::uint64_t Type = 0;

    /// "Value": the value, an integer, a float or a bool. These
    /// are the only 3 literals that will fill a cmp raw. Todo:
    /// future support for arrays? Currently they are opaque,
    /// but [2] is the same as [1 + 1].
    std::variant<numex::BigInt, numex::BigDec, bool> Val;

    /// Create a comp key part for an integer literal, using its
    /// big int internal value, and type suffix.
    static auto OfInt(numex::BigInt value, const StrView suffix) -> CompKey {
      return CompKey{.Kind = Part::Value, .Text = Str(suffix), .ParamId = 0, .Kids = {}, .Type = 0, .Val = value};
    }

    /// Create a comp key part for a float literal, using its big
    /// dec internal value, and type suffix.
    static auto OfFloat(numex::BigDec value, const StrView suffix) -> CompKey {
      return CompKey{.Kind = Part::Value, .Text = Str(suffix), .ParamId = 0, .Kids = {}, .Type = 0, .Val = value};
    }

    /// Create a comp key part for a bool literal, using the raw
    /// bool value.
    static auto OfBool(const bool value) -> CompKey {
      return CompKey{.Kind = Part::Value, .Text = {}, .ParamId = 0, .Kids = {}, .Type = 0, .Val = value};
    }

    /// A comp parameter, by its "ParamId", and a constant named
    /// through a type, the type by its "TypeId"'s word
    /// ("Box[S32]::N").
    static auto OfParam(const std::uint64_t param_id) -> CompKey {
      return CompKey{.Kind = Part::Param, .Text = {}, .ParamId = param_id, .Kids = {}, .Type = 0, .Val = {}};
    }

    /// A comp parameter, by its interned owning type, and the
    /// name that is being accessed.
    static auto OfMember(const std::uint64_t type, const StrView name) -> CompKey {
      return CompKey{.Kind = Part::Member, .Text = Str(name), .ParamId = 0, .Kids = {}, .Type = type, .Val = {}};
    }

    /// The fallback "opaque" version for all non-specialized
    /// asts.
    static auto OfOpaque(const StrView spelling) -> CompKey {
      return CompKey{.Kind = Part::Opaque, .Text = Str(spelling), .ParamId = 0, .Kids = {}, .Type = 0, .Val = {}};
    }

    /// A pack of interned elements, and an operation over two
    /// interned operands.
    static auto OfPack(std::vector<CompKey const*> elems) -> CompKey {
      return CompKey{.Kind = Part::Pack, .Text = {}, .ParamId = 0, .Kids = std::move(elems), .Type = 0, .Val = {}};
    }

    /// The binary expression where its two operands are stored
    /// as kids and the operator as the text.
    static auto OfOp(const StrView op, CompKey const *lhs, CompKey const *rhs) -> CompKey {
      return CompKey{.Kind = Part::Op, .Text = Str(op), .ParamId = 0, .Kids = {lhs, rhs}, .Type = 0, .Val = {}};
    }

    /// The reverse integer creator - extract the integer value
    /// from a node.
    SPP_ATTR_NODISCARD auto AsInt() const -> numex::BigInt const* {
      return Kind == Part::Value ? std::get_if<numex::BigInt>(&Val) : nullptr;
    }

    /// The reverse float creator - extract the float value from
    /// a node.
    SPP_ATTR_NODISCARD auto AsFloat() const -> numex::BigDec const* {
      return Kind == Part::Value ? std::get_if<numex::BigDec>(&Val) : nullptr;
    }

    /// The reverse boolean creator - extract the boolean value
    /// from a node.
    SPP_ATTR_NODISCARD auto AsBool() const -> bool const* {
      return Kind == Part::Value ? std::get_if<bool>(&Val) : nullptr;
    }

    /// Whether this node or any node below it satisfies "pred",
    /// parents before their parts.
    SPP_ATTR_NODISCARD auto Any(std::function<bool(CompKey const &)> const &pred) const -> bool {
      return pred(*this) or genex::any_of(Kids, [&pred](CompKey const *kid) { return kid->Any(pred); });
    }

    /// Visit this node and every node below it, parents before
    /// their parts.
    auto Visit(std::function<void(CompKey const &)> const &visit) const -> void {
      visit(*this);
      for (auto const *kid : Kids) { kid->Visit(visit); }
    }

    /// Two nodes are one identity exactly when they are the same
    /// tree: this is what interning compares, so it is what makes
    /// a "CompId" injective (the hash below only has to spread
    /// the nodes, never tell them apart). The parts are interned,
    /// so the same parts are the same pointers.
    auto operator==(CompKey const &that) const -> bool = default;
  };

  /// A "comp id" is a pointer to an interned comp key
  /// ("InternCompKey"), representing an interned comp value, as a
  /// "type id" is a pointer to an interned type key: the same id
  /// for the same value, meaningful within one compilation.
  SPP_EXP_CLS using CompId = CompKey const*;

  /// A node's hash, for interning ("InternCompKey"): its parts by
  /// their interned pointers, and a number by its own hash
  /// ("BigInt::Hash", "BigDec::Hash", consistent with the "=="
  /// that interning compares by). It ends with "Hash"'s own mix, so
  /// the sets it is used in need not mix it again.
  SPP_EXP_CLS struct CompKeyHash {
    using is_avalanching = void;

    auto operator()(CompKey const &node) const noexcept -> std::uint64_t {
      auto h = static_cast<std::uint64_t>(node.Kind);
      const auto mix = [&h](const std::uint64_t x) { h = (h ^ x) * 0x100000001b3ull; };
      mix(Hash<StrView>()(node.Text));
      mix(node.ParamId);
      mix(node.Type);
      mix(node.Val.index());
      if (const auto value = node.AsInt(); value != nullptr) { mix(value->Hash()); }
      if (const auto value = node.AsFloat(); value != nullptr) { mix(value->Hash()); }
      if (const auto value = node.AsBool(); value != nullptr) { mix(*value ? 2 : 1); }
      for (auto const *kid : node.Kids) { mix(reinterpret_cast<std::uintptr_t>(kid)); }
      return Hash<std::uint64_t>()(h);
    }
  };

  /// The one canonical node equal to "node" by hash. Its
  /// parts must be interned already. This says "given this
  /// comp key, if an interned one is already equal, use
  /// this interned version".
  SPP_EXP_FUN inline auto InternCompKey(CompKey &&node) -> CompId {
    static auto interned = StableSet<CompKey, CompKeyHash>();
    if (const auto hit = interned.find(node); hit != interned.end()) { return &*hit; }
    return &*interned.emplace(std::move(node)).first;
  }

  /// A lightweight stage 9 operation, where a number of
  /// operations are computable in isolation. Allows for
  /// A[1 + 2] to be the same type as A[3] etc.
  SPP_EXP_FUN auto FoldCompValues(
    StrView op, CompKey const &lhs, CompKey const &rhs) -> std::optional<CompKey>;

  /// The comp parameters an identity names ("Param" parts),
  /// by "ParamId", in order; none inside an opaque spelling,
  /// which names nothing by identity. Only difference to the
  /// type version is this is not cached.
  SPP_EXP_FUN auto ParamsNamedBy(
    CompId id) -> std::vector<std::uint64_t>;

  /// The constants an identity names through types ("Member"
  /// parts): each type's "TypeId" word and the name, in order.
  SPP_EXP_FUN auto MembersNamedBy(
    CompId id) -> std::vector<std::pair<std::uint64_t, Str>>;

  /// Whether an identity is a value all the way down: a value,
  /// or a pack of comp values (recursive check). Anything
  /// naming a comp parameter, a constant through a type, or
  /// something opaque, or an operation that does not fold
  /// ("1 / 0"), is not.
  SPP_EXP_FUN auto IsValueCompId(
    CompId id) -> bool;

  /// The integer value an identity is, as a "u64" ("numbers::ToU64"):
  /// nothing for no identity, one that is not an integer value,
  /// or one out of range. How a comp argument such as "Arr"'s
  /// "n" is read as a count.
  SPP_EXP_FUN auto U64Of(
    CompId id) -> std::optional<std::uint64_t>;

  /// "DoesTypeIdNameAnyGnParams" for a comp value: a comp
  /// parameter of its own ("ParamsNamedBy"), or a parameter of a
  /// type it names a constant through ("Box[T]::n" names "T"),
  /// as a type's "ParamsNamedBy" counts both.
  SPP_EXP_FUN auto DoesCompIdNameAnyGnParams(CompId id) -> bool;

  /// "IsConcreteTypeId" for a comp value: it names no parameter
  /// ("DoesCompIdNameAnyGnParams") and no "Self", which reaches a
  /// comp value only as the owner of a constant ("Self::n"). As
  /// for a type, a part that did not resolve is not counted
  /// against it ("IsClosedCompId" does, through
  /// "IsReadableCompId").
  SPP_EXP_FUN auto IsConcreteCompId(CompId id) -> bool;

  /// Whether a comp value names no constant through a type
  /// that cannot be read yet (an unresolved owner, or a closed
  /// one whose constant "comp_generics::FindCompMemberId"
  /// cannot reach yet) - the part "PushCompPart" marks a
  /// key unresolved for. Such a value keys differently once
  /// the constant can be read, so it is not settled;
  /// parameters are allowed.
  SPP_EXP_FUN auto IsReadableCompId(CompId id) -> bool;

  /// "IsClosedTypeId" for a comp value: concrete
  /// ("IsConcreteCompId") and readable ("IsReadableCompId").
  SPP_EXP_FUN auto IsClosedCompId(CompId id) -> bool;
}
