module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.borrows;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.regions;
import spp.asts.ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_keyword_res_ast;
import genex;
import std;

namespace spp::analyse::utils::borrows {
  namespace {
    /// [CHECKED]
    /// Raise if any escaping borrow contained by a value would
    /// out-live what it borrows from. This prevents closures
    /// and generators from lifting escaping borrows out of
    /// their genuine lifetime.
    auto EnforceEscapingBorrowsOutlive(
      Vec<Tup<Ast const*, bool, Scope*>> const &escaping_borrows, VariableSymbol const &lhs,
      Ast *owner, ScopeManager const &sm) -> void {
      // Get the initialisation scope for the left hand side.
      // This must be an ancestor of every escaping borrows'
      // symbol's definition scope.
      using errors::SppBorrowLifetimeIncreaseError;
      const auto lhs_init_scope = lhs.ScopeDefinedIn;
      if (lhs_init_scope == nullptr) { return; }

      // Check for every escaping borrow in the list.
      for (auto const &[e, _, _] : escaping_borrows) {
        const auto source_sym = sm.CurrentScope->FindVarSymbolOutermost(*e).first;
        if (source_sym == nullptr or source_sym->ScopeDefinedIn == nullptr) { continue; }

        // Raise an error if the source symbol's definition
        // scope it outliving the lhs initialisation scope.
        const auto found_at = genex::position(
          lhs_init_scope->GetAncestors(), genex::operations::eq_fixed{source_sym->ScopeDefinedIn});
        spp::RaiseIf<SppBorrowLifetimeIncreaseError>(
          found_at < 0, {sm.CurrentScope}, ERR_ARGS(*owner, *lhs.Name, *e));
      }
    }
  }
}

/// [CHECKED]
auto spp::analyse::utils::borrows::ValidateUnnamedArgumentBorrow(
  FunctionCallArgumentAst const &arg, VariableSymbol const *const sym,
  Vec<Ast const*> &borrows_ref, Vec<Ast const*> &borrows_mut,
  ScopeManager &sm, meta::CompilerMetaData *const meta) -> void {
  // Failsafe for symbolic borrows, which are already handled;
  // this handles unnamed borrow creators, like vec[mut 5] etc.
  using errors::SppMemoryOverlapUsageError;
  if (sym != nullptr) { return; }

  // Get the convention from the inference. For owned symbols,
  // return too.
  const auto conv = arg.Conv != nullptr
    ? arg.Conv->Tag()
    : arg.Val->InferTypeRef(&sm, meta).Conv;
  if (conv == ConventionTag::MOV) { return; }

  // Do the cross check here with the existing borrows, raising
  // an error on conflict.
  const auto is_mut = conv == ConventionTag::MUT;
  auto candidates = is_mut
    ? genex::views::concat(borrows_ref, borrows_mut) | genex::to<Vec>()
    : borrows_mut;

  auto overlaps = candidates
    | genex::views::filter([&arg](auto const &x) { return regions::MemRegionOverlap(*x, *arg.Val); })
    | genex::to<Vec>();

  spp::RaiseIf<SppMemoryOverlapUsageError>(
    not overlaps.IsEmpty(), {sm.CurrentScope},
    ERR_ARGS(*overlaps[0], *arg.Val));

  // Append the arg val into the borrow list, so that following
  // borrows get conflict checked with this unnamed one.
  (is_mut ? borrows_mut : borrows_ref).EmplaceBack(arg.Val.get());
}

/// [CHECKED]
auto spp::analyse::utils::borrows::PreventBorrowLifetimeExtension(
  Ast const &rhs_expr, VariableSymbol const *lhs_outermost, VariableSymbol const *rhs_outermost,
  Ast *owner, ScopeManager const &sm, const bool override_borrow) -> void {
  // Todo: A similar version of this function will be needed for
  // "return" statements as-well as the currently used "=" stms?

  // The right-hand-side is a borrow if the memory info struct
  // contains a borrow location (grab it). Allow for enforced
  // override.
  const auto is_rhs_borrow = override_borrow or (
    rhs_outermost and spp::get<0>(rhs_outermost->MemInfo->AstBorrowed) != nullptr);

  // Enter the lifetime checker, requiring valid lhs/rhs, and a
  // checked rhs borrow test.
  if (lhs_outermost != nullptr and rhs_outermost != nullptr and is_rhs_borrow) {
    // Get the borrow scope for the rhs, from the memory info.
    // Falls back to the current scope. Todo: is this needed?
    const auto has_borrow_scope = spp::get<1>(rhs_outermost->MemInfo->AstBorrowed);
    const auto rhs_borrow_scope = has_borrow_scope ? has_borrow_scope : sm.CurrentScope;
    const auto lhs_init_scope = lhs_outermost->ScopeDefinedIn;

    // Provided the lhs init scope is not nullptr, we can test
    // its depth against the borrow scope.
    // Todo: nullptr check needed?
    if (lhs_init_scope != nullptr) {
      const auto scope_depth_difference = genex::position(
        lhs_init_scope->GetAncestors(), genex::operations::eq_fixed{rhs_borrow_scope});
      const auto ast = spp::get<0>(rhs_outermost->MemInfo->AstBorrowed);
      RaiseIf<errors::SppBorrowLifetimeIncreaseError>(
        scope_depth_difference < 0, {sm.CurrentScope},
        ERR_ARGS(*owner, *lhs_outermost->Name, *(ast ? ast : &rhs_expr)));
    }
  }

  // Ensure a value that contains escaping borrows isn't
  // increasing the escaping borrows' lifetimes.
  else if (lhs_outermost != nullptr and rhs_outermost != nullptr) {
    EnforceEscapingBorrowsOutlive(
      rhs_outermost->MemInfo->AstContainedEscapingBorrows, *lhs_outermost, owner, sm);
  }

  // The same, for a non-symbolic right-hand-side expr,
  // typically for member access or function calls.
  else if (lhs_outermost != nullptr and rhs_expr.To<PostfixExpressionAst>() != nullptr) {
    EnforceEscapingBorrowsOutlive(
      lhs_outermost->MemInfo->AstContainedEscapingBorrows, *lhs_outermost, owner, sm);
  }

  // Handle the special case for the "gen.res()" generator
  // resumption method. Todo: util method for the ast check.
  else if (const auto pf = rhs_expr.To<PostfixExpressionAst>(); pf and pf->Op->To<
    PostfixExpressionOperatorKeywordResAst>()) {
    const auto new_rhs_sym = sm.CurrentScope->FindVarSymbolOutermost(*pf->Lhs).first;
    PreventBorrowLifetimeExtension(*pf->Lhs, lhs_outermost, new_rhs_sym, owner, sm, true);
  }
}
