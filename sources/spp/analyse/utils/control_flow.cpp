module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.control_flow;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.case_expression_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.expression_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.statement_ast;
import spp.asts.meta.compiler_meta_data;
import genex;
import std;

auto spp::analyse::utils::control_flow::Diverges(
  StatementAst &stmt, ScopeManager *sm, CompilerMetaData *meta) -> bool {
  // The written forms first: they need no analysis, and "ret"
  // or "exit" has no "!" type to find.
  if (stmt.Terminates()) { return true; }

  // A block diverges when its last statement does, read in the
  // block's own scope. Its type alone misses a trailing "let"
  // bound to a "!" value, which is typed "Void".
  if (const auto block = stmt.To<asts::InnerScopeExpressionAst>(); block != nullptr) {
    if (block->Members.IsEmpty()) { return false; }
    auto tm = ScopeManager(sm->GlobalScope, block->GetAstScope());
    return Diverges(*block->Members.Back(), &tm, meta);
  }

  // The same for a "case": it has to have an "else" (or it can
  // fall through), and then every branch has to diverge.
  if (const auto case_expr = stmt.To<CaseExpressionAst>(); case_expr != nullptr) {
    if (case_expr->Branches.IsEmpty()) { return false; }
    if (case_expr->Branches.Back()->Patterns[0]->To<CasePatternVariantElseAst>() == nullptr) { return false; }
    return genex::all_of(case_expr->Branches, [&](auto const &branch) { return Diverges(*branch->Body, sm, meta); });
  }

  // Otherwise it is the value's type: "!" is only met by what
  // never produces one. A "let" is not an expression, but it
  // runs its value.
  const auto let = stmt.To<LetStatementInitializedAst>();
  const auto expr = let != nullptr ? let->Val.get() : stmt.To<ExpressionAst>();
  if (expr == nullptr) { return false; }
  const auto _meta_guard = MetaGuard(meta);
  meta->IgnoreMissingElseBranchForInference = true;
  return expr->InferTypeRef(sm, meta).IsNever;
}

auto spp::analyse::utils::control_flow::ValidateNoUnreachableCode(
  StatementAst &member, StatementAst const *next, ScopeManager *sm, CompilerMetaData *meta) -> void {
  using errors::SppUnreachableCodeError;
  RaiseIf<SppUnreachableCodeError>(
    next != nullptr and Diverges(member, sm, meta),
    {sm->CurrentScope}, ERR_ARGS(member, *next));
}
