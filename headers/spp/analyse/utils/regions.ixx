module;
#include <spp/macros.hpp>

export module spp.analyse.utils.regions;
import spp.utils.interner;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::analyse::utils::regions, enum class MemRegionRelation);
use(spp::asts, struct Ast);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct IdentifierAst);

namespace spp::analyse::utils::regions {
  /// One step into a place: an attribute or a tuple element
  /// by its name, or a method called on it. A method's step
  /// is opaque: what the call hands back can be anywhere
  /// inside its receiver.
  SPP_EXP_CLS struct PlaceStep {
    spp::utils::InternedId Name;
    bool Opaque = false;
    auto operator==(PlaceStep const &) const -> bool = default;
  };

  /// A place by identity: the variable it hangs off, and the
  /// steps taken into it, outermost first ("v[mut i]" is
  /// "v.index_mut(i)": what the call hands back is derived from
  /// "v"). A dereference takes no step.
  SPP_EXP_CLS struct Place {
    VariableSymbol *Root = nullptr;
    std::vector<PlaceStep> Steps; // Interned identifier ast.
    auto operator==(Place const &) const -> bool = default;
  };

  /// A place, kept with the expression it was taken from (what
  /// an error points at). The place is read where the expression
  /// is written, so it means the variable it named there,
  /// whatever a later "let" shadows; none for an expression
  /// naming no place.
  SPP_EXP_CLS struct PlaceAt {
    Ast const *Site = nullptr;
    std::optional<Place> Of;
  };
}

SPP_EXP_CLS enum class spp::analyse::utils::regions::MemRegionRelation {
  Disjoint, // Non overlapping regions of memory: "a" vs "b" or "a.b" vs "a.c"
  Contains, // The first place has a region containing the second: "a" vs "a.b"
  ContainedBy, // The first place's region is contained by the second: "a.b" vs "a"
  Overlapping, // Neither holds the other, but an opaque step may reach the other: "a.f()" vs "a.b"
};

namespace spp::analyse::utils::regions {
  /// The place an expression names, read where "scope" reads
  /// it: its root variable ("Scope::FindVarSymbolOutermost"),
  /// then a step per attribute or method called. Nothing for
  /// a value that names no place: a temporary or a literal.
  SPP_EXP_FUN auto PlaceOf(
    Ast const &ast, Scope const &scope) -> std::optional<Place>;

  /// How two places relate: disjoint unless they hang off the
  /// same variable and one's steps begin with the other's; then
  /// the one with fewer steps contains the other ("a" contains
  /// "a.b", and a place contains itself).
  SPP_EXP_FUN auto Relate(
    Place const &p1, Place const &p2) -> MemRegionRelation;

  /// "PlaceOf" kept with the expression it was taken from. Bind
  /// the ast alongside the result of "PlaceOf" with the ast and
  /// scope.
  SPP_EXP_FUN auto PlaceAtOf(
    Ast const &ast, Scope const &scope) -> PlaceAt;

  /// Whether two places share memory: one is, or holds, the
  /// other. A value naming no place (a temporary) shares none.
  SPP_EXP_FUN auto Overlap(
    PlaceAt const &p1, PlaceAt const &p2) -> bool;

  /// A place written out as its root's name and its steps
  /// ("a.b.c").
  SPP_EXP_FUN auto PlaceText(
    Place const &place) -> Str;

  /// The first part of a value that a destructure has not
  /// accounted for, and would otherwise silently drop. Copyable
  /// parts are ignored, following usual memory rules, and a
  /// value who has all fields copyable will never provide a
  /// response here.
  SPP_EXP_FUN auto FirstUnaccountedPart(
    VariableSymbol const &sym, Place const &region, ScopeManager const &sm) -> Str;

  /// Check if an ast is an identifier ast type. This is used
  /// to determine which mutability and memory rules to apply
  /// during assignment analysis.
  SPP_EXP_FUN auto IsIdentifier(
    Ast const *expr) -> bool;

  /// Check if an ast is an attribute access ast type. This
  /// has alternative mutability/memory rules to enforce, and
  /// because it is only used from the assignment ast, we can
  /// just negate the identifier check.
  SPP_EXP_FUN auto IsAttr(
    Ast const *expr, ScopeManager const *sm) -> bool;

  /// Whether the expression holds "destructure-able" storage
  /// or not. Typically, if not, then a materialization occurs.
  SPP_EXP_FUN auto IsDestructurePlaceExpression(
    ExpressionAst const &expr) -> bool;

  /// The type of the place "steps" names after "count" of its
  /// steps ("type_members::GetAllParts"), and the scope it
  /// resolves in; a count of zero is the root itself. No type when
  /// a step has no such part.
  SPP_EXP_FUN auto DescendToPart(
    TypeRef const &root, Scope const &root_scope, std::vector<PlaceStep> const &steps,
    std::size_t count) -> Pair<TypeRef, Scope const*>;

  /// Whether everything the place "region" owns has been
  /// consumed, given every partial move recorded against its
  /// symbol, either directly or by all of its non-copyable parts.
  /// The first part left over is written to "unaccounted".
  SPP_EXP_FUN auto RegionConsumed(
    Place const &region, TypeRef const &type, Scope const &scope, Vec<PlaceAt> const &moves,
    Str *unaccounted = nullptr, bool descend_regardless = false) -> bool;
}
