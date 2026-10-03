module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.comp_key;
import spp.utils.types;
import genex;
import std;
import numex.big_int;

/// A comp value's identity ("comp_generics::CompKey") is text over a small grammar, so two values compare by
/// meaning: a value ("V2_uz", "Vtrue"), a comp parameter ("C12"), a pack ("P(V1_uz, C12)"), an operation over two of
/// them ("(C12 + V1_uz)"), a constant named through a type ("M<type>.mo_seq_cst", the type by its "TypeId", so it is
/// rewritten as types are), or anything else as its spelling ("O5:x.y()", length-prefixed so it parses).
/// This module reads, rewrites and folds that text without an ast, below "instance_key", whose substitution of a
/// type's identity rewrites the comp arguments in it.
namespace spp::analyse::scopes {
  /// An identity, parsed.
  SPP_EXP_CLS struct CompNode {
    /// Which part of the grammar a node is.
    enum class Part : std::uint8_t { Value, Param, Pack, Op, Opaque, Member };

    Part Kind = Part::Value;

    /// "Value": the literal ("2_uz", "-3_s32", "true"); "Op": the operator ("+"); "Opaque": the spelling; "Member": the
    /// constant's name.
    Str Text;

    /// "Param": the comp parameter's "ParamId".
    std::uint64_t ParamId = 0;

    /// "Pack": the elements; "Op": the two operands.
    std::vector<CompNode> Kids;

    /// "Member": the type the constant is named through, as its "TypeId"'s word.
    std::uint64_t Type = 0;

    /// A comp parameter, by its "ParamId" ("C12"), and a constant named through a type, the type by its "TypeId"'s
    /// word ("Box[S32]::N").
    static auto OfParam(const std::uint64_t param_id) -> CompNode {
      return CompNode{.Kind = Part::Param, .Text = {}, .ParamId = param_id, .Kids = {}, .Type = 0};
    }

    static auto OfMember(const std::uint64_t type, const StrView name) -> CompNode {
      return CompNode{.Kind = Part::Member, .Text = Str(name), .ParamId = 0, .Kids = {}, .Type = type};
    }

    /// Whether this node or any node below it satisfies "pred", parents before their parts.
    SPP_ATTR_NODISCARD auto Any(std::function<bool(CompNode const &)> const &pred) const -> bool {
      return pred(*this) or genex::any_of(Kids, [&pred](CompNode const &kid) { return kid.Any(pred); });
    }

    /// Visit this node and every node below it, parents before their parts.
    auto Visit(std::function<void(CompNode const &)> const &visit) const -> void {
      visit(*this);
      for (auto const &kid : Kids) { kid.Visit(visit); }
    }
  };

  /// Parse an identity; nothing when the text is not one.
  SPP_EXP_FUN auto ParseCompKey(StrView text) -> std::optional<CompNode>;

  /// Print a parsed identity back as its text.
  SPP_EXP_FUN auto PrintCompKey(CompNode const &node) -> Str;

  /// An operation over two values (literals as an identity's "Value" spells them), folded as comp values fold: integers
  /// by "BigInt" arithmetic, typed by suffix (an unsuffixed side takes the other side's type, and two different types do
  /// not mix), bools by equality only. Nothing when the operation does not fold (by zero, an unknown operator, a shift
  /// past an unknown type).
  SPP_EXP_FUN auto FoldCompValues(StrView op, StrView lhs, StrView rhs) -> std::optional<Str>;

  /// An identity with each comp parameter "bound" answers for replaced by that identity, at any depth, a pack parameter
  /// standing as one element of a pack spread into the elements it is bound to ("is_pack"), a constant named through a
  /// type replaced by what "member" answers for it (its type rewritten, or the value it then folds to; nothing from it
  /// is no identity), and every operation whose operands are then values folded. With no binders, only folded.
  SPP_EXP_FUN auto RewriteCompKey(
    CompNode const &node,
    std::function<std::optional<CompNode>(std::uint64_t)> const &bound = nullptr,
    std::function<bool(std::uint64_t)> const &is_pack = nullptr,
    std::function<std::optional<CompNode>(std::uint64_t, StrView)> const &member = nullptr) -> std::optional<CompNode>;

  /// An integer value's literal ("-3_s32", "2_" unsuffixed) as its value and type suffix; nothing for anything else.
  SPP_EXP_FUN auto ParseCompInt(StrView literal) -> std::optional<Pair<numex::BigInt, Str>>;

  /// A bool value's literal ("true", "false"); nothing for anything else.
  SPP_EXP_FUN auto ParseCompBool(StrView literal) -> std::optional<bool>;

  /// The comp parameters an identity names ("C<ParamId>" parts), by "ParamId", in order; none inside an opaque
  /// spelling, which names nothing by identity.
  SPP_EXP_FUN auto CompKeyParams(CompNode const &node) -> std::vector<std::uint64_t>;

  /// The constants an identity names through types ("Member" parts): each type's "TypeId" word and the name, in order.
  SPP_EXP_FUN auto CompKeyMembers(CompNode const &node) -> std::vector<std::pair<std::uint64_t, Str>>;
}
