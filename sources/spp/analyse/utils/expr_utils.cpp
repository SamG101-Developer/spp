module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.expr_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.case_expression_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.expression_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ice;
import genex;
import std;

auto spp::analyse::utils::expr_utils::IsPrimaryExprTypeValid(
  ExpressionAst const &expr, ScopeManager const &sm,
  PrimaryExpressionOptions &&options) -> bool {
  // Only allow types if types are explicitly allowed, or
  // are zero type. "()" in value position is the empty
  // tuple's type, which has exactly one value, like any
  // zero type.
  if (not options.AllowTypeAst and expr.To<asts::TypeAst>() != nullptr) {
    const auto type_sym = sm.CurrentScope->FindTypeSymbol(expr.To<asts::TypeAst>());
    return type_sym != nullptr and (type_sym->IsZeroType()
      or (type_predicates::IsTypeTuple(TypeRef::ForKindCheck(*type_sym, *sm.CurrentScope), *sm.CurrentScope)
        and type_sym->TypeArgRefs().empty()));
  }

  // Only allow tokens when they're explicit allowed,
  // like "5 + .."
  if (expr.To<asts::TokenAst>() != nullptr) { return false; }
  return true;
}

auto spp::analyse::utils::expr_utils::ValidateDiscardedValue(
  Ast &member, Scope *scope, ScopeManager const &sm,
  CompilerMetaData *const meta) -> void {
  using errors::SppDiscardedValueError;
  using type_predicates::IsTypeVoid;

  if (scope == nullptr) { return; }

  // A "case" written as a statement discards whatever its branches
  // end on, so that is where the discard is: reporting the "case"
  // itself would point at the wrong thing and say nothing about
  // which branch is at fault.
  if (auto const *case_expr = member.To<CaseExpressionAst>()) {
    for (auto const &branch : case_expr->Branches) {
      if (branch->Body == nullptr or branch->Body->Members.empty()) { continue; }
      ValidateDiscardedValue(*branch->Body->FinalMember(), branch->Body->GetAstScope(), sm, meta);
    }
    return;
  }

  // Same for a bare block, whose value is its final statement.
  if (auto const *block = member.To<InnerScopeExpressionAst>()) {
    if (block->Members.empty()) { return; }
    ValidateDiscardedValue(*block->FinalMember(), block->GetAstScope(), sm, meta);
    return;
  }

  // Only an expression produces a value at all; a "let" or an
  // assignment is a statement and has nothing to discard.
  const auto expr = member.To<StatementAst>();
  if (expr == nullptr or expr->To<ExpressionAst>() == nullptr) { return; }

  // Inferred against the scope the statement was written in, which
  // is not necessarily the one the walk is currently sitting in.
  auto tm = sm.NewManagerAt(scope);
  const auto type = [&] {
    const auto _meta_guard = MetaGuard(meta);
    meta->IgnoreMissingElseBranchForInference = true;
    return expr->InferType(&tm, meta);
  }();
  if (type == nullptr) { Ice(member, *scope, "discarded expression has no inferred type"); }

  const auto type_name = type->ToString();
  if (IsTypeVoid(TypeRef::ForKindCheck(*type, *scope), *scope) or type->IsNeverType()) { return; }
  Raise<SppDiscardedValueError>({scope}, ERR_ARGS(member, StrView(type_name)));
}

/// [CHECKED]
auto spp::analyse::utils::expr_utils::Diverges(
  StatementAst &stmt, ScopeManager *sm, CompilerMetaData *meta) -> bool {
  // The written forms first: they need no analysis, and "ret"
  // or "exit" has no "!" type to find.
  if (stmt.Terminates()) { return true; }

  // A block diverges when its last statement does, read in the
  // block's own scope. Its type alone misses a trailing "let"
  // bound to a "!" value, which is typed "Void".
  if (const auto block = stmt.To<asts::InnerScopeExpressionAst>(); block != nullptr) {
    if (block->Members.empty()) { return false; }
    auto tm = sm->NewManagerAt(block->GetAstScope());
    return Diverges(*block->Members.back(), &tm, meta);
  }

  // The same for a "case": it has to have an "else" (or it can
  // fall through), and then every branch has to diverge.
  if (const auto case_expr = stmt.To<CaseExpressionAst>(); case_expr != nullptr) {
    if (case_expr->Branches.empty()) { return false; }
    if (case_expr->Branches.back()->Patterns[0]->To<CasePatternVariantElseAst>() == nullptr) { return false; }
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
