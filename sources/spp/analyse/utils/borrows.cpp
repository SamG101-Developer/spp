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
    /**
     * Raise if any borrow a value carries would out-live what it borrows from, once that value is held by @p lhs .
     *
     * @n
     * A coroutine handle keeps the borrows its call was given alive for as long as the handle lives, so putting one
     * into a symbol declared further out moves those borrows past the frame that owns what they point at. Each is
     * checked on its own: one borrow out-living its source is enough, however many the handle carries.
     *
     * @param escaping_borrows The borrows the value carries.
     * @param lhs The symbol the value is being put into.
     * @param owner The ast to report the error against.
     * @param sm The scope manager, for resolving each borrow's source.
     */
    auto EnforceEscapingBorrowsOutlive(
      Vec<Tup<Ast const*, bool, Scope*>> const &escaping_borrows,
      VariableSymbol const &lhs,
      Ast *owner,
      ScopeManager const &sm)
      -> void {
      //
      using errors::SppBorrowLifetimeIncreaseError;
      const auto lhs_init_scope = lhs.ScopeDefinedIn;
      if (lhs_init_scope == nullptr) { return; }

      for (auto const &[e, _, _] : escaping_borrows) {
        const auto source_sym = sm.CurrentScope->GetVarSymbolOutermost(*e).first;
        if (source_sym == nullptr or source_sym->ScopeDefinedIn == nullptr) { continue; }

        // The source out-lives the destination exactly when its scope is one the destination sits inside of, which is
        // what finding it among the destination's ancestors says.
        const auto found_at = genex::position(
          lhs_init_scope->Ancestors(), genex::operations::eq_fixed{source_sym->ScopeDefinedIn});
        spp::RaiseIf<SppBorrowLifetimeIncreaseError>(
          found_at < 0, {sm.CurrentScope}, ERR_ARGS(*owner, *lhs.Name, *e));
      }
    }
  }
}

auto spp::analyse::utils::borrows::ValidateUnnamedArgumentBorrow(
  FunctionCallArgumentAst const &arg,
  VariableSymbol const *const sym,
  Vec<Ast const*> &borrows_ref,
  Vec<Ast const*> &borrows_mut,
  ScopeManager &sm,
  meta::CompilerMetaData *const meta)
  -> void {
  //
  using errors::SppMemoryOverlapUsageError;

  // A borrow with a name is one the caller's own branches
  // take, or one being passed along rather than taken here.
  // Only the nameless case is this one's.
  if (sym != nullptr) { return; }

  // The convention as written, or, where nothing is written,
  // the one the argument's type carries - which is where a
  // subscript keeps it.
  const auto conv = arg.Conv != nullptr ? arg.Conv->Tag() : arg.Val->InferTypeRef(&sm, meta).Conv;
  if (conv == ConventionTag::MOV) { return; }

  // A mutable borrow meets every other borrow of the region;
  // an immutable one meets only a mutable.
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

  (is_mut ? borrows_mut : borrows_ref).EmplaceBack(arg.Val.get());
}

auto spp::analyse::utils::borrows::PreventBorrowLifetimeExtension(
  Ast const &rhs_expr,
  VariableSymbol const *lhs_outermost,
  VariableSymbol const *rhs_outermost,
  Ast *owner,
  ScopeManager const &sm,
  const bool override_borrow)
  -> void {
  // Todo: A similar version of this function will be needed for "return" statements as-well as the currently used "="
  //  statements.

  // Prevent a borrow being placed into a value with a longer
  // lifetime.
  const auto is_rhs_borrow = override_borrow or (
    rhs_outermost and spp::get<0>(rhs_outermost->MemInfo->AstBorrowed) != nullptr);
  if (lhs_outermost != nullptr and rhs_outermost != nullptr and is_rhs_borrow) {
    const auto has_borrow_scope = spp::get<1>(rhs_outermost->MemInfo->AstBorrowed);
    const auto rhs_borrow_scope = has_borrow_scope ? has_borrow_scope : sm.CurrentScope;
    const auto lhs_init_scope = lhs_outermost->ScopeDefinedIn;
    if (lhs_init_scope != nullptr) {
      const auto scope_depth_difference = genex::position(
        lhs_init_scope->Ancestors(), genex::operations::eq_fixed{rhs_borrow_scope});
      const auto has_borrow_ast = spp::get<0>(rhs_outermost->MemInfo->AstBorrowed);
      RaiseIf<errors::SppBorrowLifetimeIncreaseError>(
        scope_depth_difference < 0, {sm.CurrentScope},
        ERR_ARGS(*owner, *lhs_outermost->Name, *(has_borrow_ast ? has_borrow_ast : &rhs_expr)));
    }
  }

  // Ensure a value that contains escaping borrows isn't
  // increasing the escaping borrows' lifetimes.
  else if (lhs_outermost != nullptr and rhs_outermost != nullptr) {
    EnforceEscapingBorrowsOutlive(
      rhs_outermost->MemInfo->AstContainedEscapingBorrows, *lhs_outermost, owner, sm);
  }

  // The same, for a right-hand side that names no symbol of
  // its own. A call written straight into the destination
  // ("x = c(&s)") has nowhere to record what it carries but
  // the destination itself, so the borrows are read back off
  // there rather than off a handle the source never bound.
  else if (lhs_outermost != nullptr and rhs_expr.To<PostfixExpressionAst>() != nullptr) {
    EnforceEscapingBorrowsOutlive(
      lhs_outermost->MemInfo->AstContainedEscapingBorrows, *lhs_outermost, owner, sm);
  }

  // Ensure a value that contains escaping borrows isn't
  // increasing the escaping borrows' lifetimes for "gen.res()"
  // As the borrow is a temporary (no scope), the topmost
  // branch uses "current scope".
  else if (const auto pf = rhs_expr.To<PostfixExpressionAst>(); pf and pf->Op->To<
    PostfixExpressionOperatorKeywordResAst>()) {
    const auto new_rhs_sym = sm.CurrentScope->GetVarSymbolOutermost(*pf->Lhs).first;
    PreventBorrowLifetimeExtension(*pf->Lhs, lhs_outermost, new_rhs_sym, owner, sm, true);
  }
}
