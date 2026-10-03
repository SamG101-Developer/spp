module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.regions;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_members;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import std;

namespace spp::analyse::utils::regions {
  namespace {
    /// Todo: Inline with the single caller.
    auto SameRegionSection(
      IdentifierAst const &step,
      IdentifierAst const *other)
      -> bool {
      // Compare identifier name ids.
      return step.NameId() == other->NameId();
    }
  }
}

/// [CHECKED]
auto spp::analyse::utils::regions::RegionPath(
  Ast const &ast)
  -> Vec<IdentifierAst*> {
  // Get the expression parts from the ast, provided it casts
  // validly to the expression ast variant.
  const auto expr = ast.To<ExpressionAst>();
  return expr != nullptr ? expr->ExprParts() : Vec<IdentifierAst*>();
}

/// [CHECKED]
auto spp::analyse::utils::regions::MemRegionRelate(
  Vec<IdentifierAst*> const &r1, Vec<IdentifierAst*> const &r2) -> MemRegionRelation {
  // Failsafe - nothing to name is nothing to share: a
  // temporary owns a region no other expression has a
  // spelling for. This should never happen.
  if (r1.IsEmpty() or r2.IsEmpty()) { return MemRegionRelation::Disjoint; }

  // Iterate through the two paths and look for a mismatch
  // at an equal level, ie "a" vs "b", or "a.b" vs "a.c" on
  // the second part.
  for (auto i = 0uz; i < std::min(r1.Len(), r2.Len()); ++i) {
    if (not SameRegionSection(*r1[i], r2[i])) { return MemRegionRelation::Disjoint; }
  }

  // If there were no equal-level mismatches, then by length
  // check who contains who. Two regions of the same path ie
  // "a" and "a" are marked as "contains".
  return r1.Len() <= r2.Len()
    ? MemRegionRelation::Contains
    : MemRegionRelation::ContainedBy;
}

auto spp::analyse::utils::regions::MemRegionOverlap(
  Ast const &ast_1, Ast const &ast_2) -> bool {
  // Either holding the other is an overlap, so anything
  // but "no relation" is one.
  return MemRegionRelate(RegionPath(ast_1), RegionPath(ast_2)) !=
    MemRegionRelation::Disjoint;
}

auto spp::analyse::utils::regions::FirstUnaccountedPart(
  VariableSymbol const &sym,
  Vec<IdentifierAst*> const &region,
  ScopeManager const &sm)
  -> Str {
  // No region parts -> no unaccounted parts. Simple optimization
  // guard.
  if (sym.Type == nullptr or region.IsEmpty()) { return Str(); }

  // Walk the whole region, so that "a.b.c" lands on the type of
  // "c" and the scope that type resolves in.
  const auto [region_ref, region_scope] = DescendToPart(
    TypeRef::Of(*sym.Type, *sm.CurrentScope), *sm.CurrentScope, region, region.Len() - 1);
  if (region_ref.Symbol == nullptr or region_scope == nullptr) { return Str(); }

  // The caller has established that the pattern took this place
  // apart, so its parts are walked whether or not any of them
  // recorded a move.
  auto unaccounted = Str();
  auto _ = RegionConsumed(
    region, region_ref, *region_scope, sym.MemInfo->AstPartialMoves, &unaccounted, true);
  return unaccounted;
}

auto spp::analyse::utils::regions::IsIdentifier(
  Ast const *expr) -> bool {
  // Determine if the AST node is an identifier.
  return expr->To<IdentifierAst>() != nullptr;
}

auto spp::analyse::utils::regions::IsAttr(
  Ast const *expr, ScopeManager const *sm) -> bool {
  // Determine if the AST node is an attribute (ie not
  // an identifier).
  const auto *const postfix = expr->To<PostfixExpressionAst>();
  if (postfix == nullptr) { return false; }
  if (postfix->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>() == nullptr) { return false; }

  // Perform validation on the actual attribute too.
  auto const var_symbol_outermost = sm->CurrentScope->FindVarSymbolOutermost(*expr);
  return var_symbol_outermost.first != nullptr;
}

auto spp::analyse::utils::regions::IsDeref(
  Ast const *expr) -> bool {
  // Determine if the AST node is a deref op (ie not
  // an identifier or an attribute).
  const auto *const postfix = expr->To<PostfixExpressionAst>();
  if (postfix == nullptr) { return false; }

  // Check the operator on the postfix expression ast
  // node.
  return postfix->Op->To<PostfixExpressionOperatorDerefAst>() != nullptr;
}

auto spp::analyse::utils::regions::IsDestructurePlaceExpression(
  ExpressionAst const &expr) -> bool {
  // Strip the member accesses off the expression: "a.b.c"
  // becomes "a". Any other postfix operator (a function call,
  // an early return etc) means the expression produces a new
  // value rather than naming existing storage.
  auto cur = static_cast<Ast const*>(&expr);
  while (auto const *postfix = cur->To<PostfixExpressionAst>()) {
    if (postfix->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>() == nullptr) { return false; }
    cur = postfix->Lhs.get();
  }

  return cur->To<IdentifierAst>() != nullptr;
}

/**
 * The type of the place @p steps names after @p count of its steps, and the scope it resolves in. Step zero is the
 * root itself, so a @p count of zero is the root's own type and a count of @c {steps.Len() - 1} is the place the whole
 * path names. Each step is a part ("type_members::GetAllParts"): an attribute's own name for a struct, an element's
 * index for a tuple or an array. Nothing when any step along the way has no such part.
 */
auto spp::analyse::utils::regions::DescendToPart(
  TypeRef const &root,
  Scope const &root_scope,
  Vec<IdentifierAst*> const &steps,
  const std::size_t count)
  -> Pair<TypeRef, Scope const*> {
  auto part = Pair<TypeRef, Scope const*>{root, &root_scope};
  for (auto i = std::size_t{1}; i <= count and i < steps.Len(); ++i) {
    if (part.first.Symbol == nullptr or part.second == nullptr) { break; }
    auto next = Pair<TypeRef, Scope const*>{};
    for (auto const &[step, _, type, ref, where] : type_members::GetAllParts(part.first, *part.second)) {
      if (step->NameId() == steps[i]->NameId()) {
        next = {ref, where};
        break;
      }
    }
    part = next;
  }
  return part;
}

/**
 * Whether everything a place owns has been consumed, given every partial move recorded against the symbol it
 * hangs off. A move accounts for a place directly when it names the place or something containing it. A place is
 * also accounted for piecemeal, by its parts: a case pattern never marks the value it destructures as moved, only
 * each element it binds, so "case p is Outer(i=Inner(a, b), y)" records "p.i.a", "p.i.b" and "p.y", and "p.i" is
 * only covered by finding that both of its own parts were taken. The parts of a class are its attributes; the
 * parts of a tuple or an array are its elements, which a destructure records by index.
 * @param region The names of the steps of the place being accounted for, outermost first.
 * @param type That place's type.
 * @param scope The scope @p type resolves in.
 * @param moves Every partial move recorded against the owning symbol.
 * @return Whether the place has nothing left to consume.
 */
auto spp::analyse::utils::regions::RegionConsumed(
  Vec<IdentifierAst*> const &region,
  TypeRef const &type,
  Scope const &scope,
  Vec<Ast const*> const &moves,
  Str *const unaccounted,
  const bool descend_regardless)
  -> bool {
  // If there is a registered move of the entire region, then
  // a full consume has been done, so return true. Otherwise,
  // detect if a different move contains the region.
  auto touched = false;
  for (auto const *move : moves) {
    const auto relation = regions::MemRegionRelate(regions::RegionPath(*move), region);
    if (relation == regions::MemRegionRelation::Contains) { return true; }
    if (relation == regions::MemRegionRelation::ContainedBy) { touched = true; }
  }

  // If the region is not contained by any moves, then we can
  // skip individual checks, and return false here; an early
  // return optimization.
  if (not touched and not descend_regardless) {
    if (unaccounted != nullptr and unaccounted->empty()) {
      for (auto const *step : region) { *unaccounted += unaccounted->empty() ? step->Val : "." + step->Val; }
    }
    return false;
  }

  auto part = region;
  part.EmplaceBack(nullptr); // Extra spot for temp "final" part.

  // Each part is checked on its own, under the name a destructure would have recorded it by - an attribute's own
  // name, or an element's index. A copyable part was never owed to anyone, so it never has to be accounted for.
  // If any part is unaccounted for, then nor is the value holding it.
  for (auto const &[step, _, part_type, part_ref, part_scope] : type_members::GetAllParts(type, scope)) {
    if (part_ref.Symbol == nullptr or part_ref.Symbol->IsCopyable()) { continue; }
    part.Back() = step.get();
    if (not RegionConsumed(part, part_ref, *part_scope, moves, unaccounted)) { return false; }
  }

  // Nothing left behind, so at this point we know the value
  // is empty via all its partial moves.
  return true;
}
