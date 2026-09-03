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
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.loop_control_flow_statement_ast;
import spp.asts.ret_statement_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.strings;
import genex;
import std;

auto spp::analyse::utils::expr_utils::IsPrimaryExprTypeValid(
  asts::ExpressionAst const &expr,
  scopes::ScopeManager const &sm,
  PrimaryExpressionOptions &&options)
  -> bool {
  // Only allow types if types are explicitly allowed, or
  // are zero type.
  if (not options.AllowTypeAst and expr.To<asts::TypeAst>() != nullptr) {
    const auto type_sym = sm.CurrentScope->GetTypeSymbol(expr.To<asts::TypeAst>());
    return type_sym->IsZeroType();
  }

  // Only allow tokens when they're explicit allowed,
  // like "5 + .."
  if (not options.AllowTokenAst and expr.To<asts::TokenAst>() != nullptr) { return false; }
  return true;
}

auto spp::analyse::utils::expr_utils::ValidateNoUnreachableCode(
  Vec<asts::StatementAst*> const &members,
  scopes::ScopeManager const &sm)
  -> void {
  //
  using errors::SppUnreachableCodeError;

  // Check for statements after a terminating statement
  // has been reached.
  for (auto const &[i, member] : members | genex::views::enumerate) {
    const auto ret_stmt = member->To<asts::RetStatementAst>();
    const auto loop_flow_stmt = member->To<asts::LoopControlFlowStatementAst>();
    RaiseIf<SppUnreachableCodeError>(
      (ret_stmt or loop_flow_stmt) and (member != members.Back()),
      {sm.CurrentScope}, ERR_ARGS(*member, *members[i + 1]));
  }
}

auto spp::analyse::utils::expr_utils::ValidateDiscardedValue(
  asts::Ast &member,
  scopes::Scope * scope,
  scopes::ScopeManager const &sm,
  asts::meta::CompilerMetaData *const meta)
  -> void {
  //
  using errors::SppDiscardedValueError;
  using type_predicates::IsTypeNever;
  using type_predicates::IsTypeVoid;

  if (scope == nullptr) { return; }

  // A "case" written as a statement discards whatever its branches
  // end on, so that is where the discard is: reporting the "case"
  // itself would point at the wrong thing and say nothing about
  // which branch is at fault.
  if (auto const *case_expr = member.To<asts::CaseExpressionAst>()) {
    for (auto const &branch : case_expr->Branches) {
      if (branch->Body == nullptr or branch->Body->Members.IsEmpty()) { continue; }
      ValidateDiscardedValue(*branch->Body->FinalMember(), branch->Body->GetAstScope(), sm, meta);
    }
    return;
  }

  // Same for a bare block, whose value is its final statement.
  if (auto const *block = member.To<asts::InnerScopeExpressionAst>()) {
    if (block->Members.IsEmpty()) { return; }
    ValidateDiscardedValue(*block->FinalMember(), block->GetAstScope(), sm, meta);
    return;
  }

  // Only an expression produces a value at all; a "let" or an
  // assignment is a statement and has nothing to discard.
  const auto expr = member.To<asts::StatementAst>();
  if (expr == nullptr or expr->To<asts::ExpressionAst>() == nullptr) { return; }

  // Inferred against the scope the statement was written in, which
  // is not necessarily the one the walk is currently sitting in.
  auto tm = scopes::ScopeManager(sm.GlobalScope, scope);
  const auto type = [&] {
    const auto _meta_guard = asts::meta::MetaGuard(meta);
    meta->IgnoreMissingElseBranchForInference = true;
    return expr->InferType(&tm, meta);
  }();
  if (type == nullptr) { return; }

  const auto type_name = type->ToString();
  if (IsTypeVoid(*type, *scope) or IsTypeNever(*type, *scope)) { return; }
  // Todo: See the note on the same environment variable in
  //  "linear_utils.cpp" - a migration aid, to be removed once
  //  the standard library is linear-clean.
  if (std::getenv("SPP_LINEAR_SURVEY") != nullptr) {
    try { Raise<SppDiscardedValueError>({scope}, ERR_ARGS(member, StrView(type_name))); }
    catch (errors::SemanticError const &e) { std::cerr << "LINEAR|" << e.what() << "\n"; }
    return;
  }
  Raise<SppDiscardedValueError>({scope}, ERR_ARGS(member, StrView(type_name)));
}

auto spp::analyse::utils::expr_utils::RaiseMissingIdentifierAndClosestOptions(
  asts::IdentifierAst const &identifier,
  Vec<scopes::VariableSymbol*> const &var_symbols,
  Vec<scopes::NamespaceSymbol*> const &ns_symbols,
  scopes::ScopeManager const &sm)
  -> void {
  //
  using spp::utils::strings::ClosestMatch;
  using errors::SppIdentifierUnknownError;

  //
  const auto v_alternatives = var_symbols
    | genex::views::transform([](auto const &x) { return x->Name->Val; })
    | genex::to<Vec>();
  const auto ns_alternatives = ns_symbols
    | genex::views::transform([](auto const &x) { return x->Name->Val; })
    | genex::to<Vec>();
  const auto alternatives = genex::views::concat(v_alternatives, ns_alternatives) | genex::to<Vec>();

  //
  const auto closest_match = ClosestMatch(identifier.Val, alternatives);
  Raise<SppIdentifierUnknownError>(
    {sm.CurrentScope}, ERR_ARGS(identifier, "identifier", closest_match));
}

auto spp::analyse::utils::expr_utils::RaiseMissingTypeIdentifierAndClosestOptions(
  asts::TypeIdentifierAst const &identifier,
  Vec<scopes::TypeSymbol*> const &symbols,
  scopes::ScopeManager const &sm)
  -> void {
  //
  using spp::utils::strings::ClosestMatch;

  //
  const auto alternatives = symbols
    | genex::views::filter([](auto const &x) { return not x->Name->IsCompilerGeneratedType(); })
    | genex::views::transform([](auto const &x) { return x->Name->Name; })
    | genex::to<Vec>();

  //
  const auto closest_match = ClosestMatch(identifier.Name, alternatives);
  Raise<errors::SppIdentifierUnknownError>(
    {sm.CurrentScope}, ERR_ARGS(identifier, "type identifier", closest_match));
}
