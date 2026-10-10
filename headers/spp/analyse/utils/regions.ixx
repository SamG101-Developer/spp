module;
#include <spp/macros.hpp>

export module spp.analyse.utils.regions;
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
};

namespace spp::analyse::utils::regions {
  /// Get the parts of a value that form the "memory region"
  /// that it represents. For example, the region "a.b.c" is
  /// represented by ["a", "b", "c"].
  SPP_EXP_FUN auto RegionPath(Ast const &ast) -> Vec<IdentifierAst*>;

  /// How 2 regions relate to one another. Compare each region
  /// for inequality implying disjointedness. If they are equal,
  /// the shorter path "contains" the longer one.
  SPP_EXP_FUN auto MemRegionRelate(Vec<IdentifierAst*> const &r1, Vec<IdentifierAst*> const &r2) -> MemRegionRelation;

  /// Two regions overlap if their relation is not "disjoint",
  /// so just wrap the memory region relation function, with
  /// "ast->region" conversion too.
  SPP_EXP_FUN auto MemRegionOverlap(Ast const &ast_1, Ast const &ast_2) -> bool;

  /// The first part of a value that a destructure has not
  /// accounted for, and would otherwise silently drop. Copyable
  /// parts are ignored, following usual memory rules, and a
  /// value who has all fields copyable will never provide a
  /// response here. Todo: region->ast (and all calls using it),
  /// to normalize with other functions in this module.
  SPP_EXP_FUN auto FirstUnaccountedPart(
    VariableSymbol const &sym,
    Vec<IdentifierAst*> const &region,
    ScopeManager const &sm)
    -> Str;

  /// Check if an ast is an identifier ast type. This is used
  /// to determine which mutability and memory rules to apply
  /// during assignment analysis.
  SPP_EXP_FUN auto IsIdentifier(Ast const *expr) -> bool;

  /// Check if an ast is an attribute access ast type. This
  /// has alternative mutability/memory rules to enforce, and
  /// because it is only used from the assignment ast, we can
  /// just negate the identifier check.
  SPP_EXP_FUN auto IsAttr(Ast const *expr, ScopeManager const *sm) -> bool;

  /// Whether the expression holds "destructure-able" storage
  /// or not. Typically, if not, then a materialization occurs.
  SPP_EXP_FUN auto IsDestructurePlaceExpression(
    ExpressionAst const &expr)
    -> bool;

  /// The type of the place "steps" names after "count" of its
  /// steps ("type_members::GetAllParts"), and the scope it
  /// resolves in. Step zero is the root itself. No type when a
  /// step has no such part.
  SPP_EXP_FUN auto DescendToPart(
    TypeRef const &root,
    Scope const &root_scope,
    Vec<IdentifierAst*> const &steps,
    const std::size_t count)
    -> Pair<TypeRef, Scope const*>;

  /// Whether everything the place "region" owns has been
  /// consumed, given every partial move recorded against its
  /// symbol, either directly or by all of its non-copyable parts.
  /// The first part left over is written to "unaccounted".
  SPP_EXP_FUN auto RegionConsumed(
    Vec<IdentifierAst*> const &region,
    TypeRef const &type,
    Scope const &scope,
    Vec<Ast const*> const &moves,
    Str *const unaccounted = nullptr,
    const bool descend_regardless = false)
    -> bool;

}
