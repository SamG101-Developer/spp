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
import spp.asts.expression_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import std;

namespace spp::analyse::utils::expr_utils {
  namespace {
  }
}

auto spp::analyse::utils::expr_utils::IsPrimaryExprTypeValid(
  ExpressionAst const &expr, ScopeManager const &sm,
  PrimaryExpressionOptions &&options) -> bool {
  // Only allow types if types are explicitly allowed, or
  // are zero type. "()" in value position is the empty
  // tuple's type, which has exactly one value, like any
  // zero type.
  if (not options.AllowTypeAst and expr.To<asts::TypeAst>() != nullptr) {
    const auto type_sym = sm.CurrentScope->GetTypeSymbol(expr.To<asts::TypeAst>());
    return type_sym != nullptr and (type_sym->IsZeroType()
      or (type_predicates::IsTypeTup(*type_sym, *sm.CurrentScope) and type_sym->TypeArgTypes().IsEmpty()));
  }

  // Only allow tokens when they're explicit allowed,
  // like "5 + .."
  if (not options.AllowTokenAst and expr.To<asts::TokenAst>() != nullptr) { return false; }
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
      if (branch->Body == nullptr or branch->Body->Members.IsEmpty()) { continue; }
      ValidateDiscardedValue(*branch->Body->FinalMember(), branch->Body->GetAstScope(), sm, meta);
    }
    return;
  }

  // Same for a bare block, whose value is its final statement.
  if (auto const *block = member.To<InnerScopeExpressionAst>()) {
    if (block->Members.IsEmpty()) { return; }
    ValidateDiscardedValue(*block->FinalMember(), block->GetAstScope(), sm, meta);
    return;
  }

  // Only an expression produces a value at all; a "let" or an
  // assignment is a statement and has nothing to discard.
  const auto expr = member.To<StatementAst>();
  if (expr == nullptr or expr->To<ExpressionAst>() == nullptr) { return; }

  // Inferred against the scope the statement was written in, which
  // is not necessarily the one the walk is currently sitting in.
  auto tm = ScopeManager(sm.GlobalScope, scope);
  const auto type = [&] {
    const auto _meta_guard = MetaGuard(meta);
    meta->IgnoreMissingElseBranchForInference = true;
    return expr->InferType(&tm, meta);
  }();
  if (type == nullptr) { return; }

  const auto type_name = type->ToString();
  if (IsTypeVoid(TypeRef::OfHead(*type, *scope), *scope) or type->IsNeverType()) { return; }
  Raise<SppDiscardedValueError>({scope}, ERR_ARGS(member, StrView(type_name)));
}
