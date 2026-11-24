module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>
#include <spp/parse/macros.hpp>

module spp.analyse.utils.expr_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.drop_utils;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.case_expression_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.lexer;
import spp.parse.parser_spp;
import spp.utils.ice;
import spp.utils.uid;
import genex;
import std;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, Scope);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::asts, Ast);
use_alias(spp::asts, CaseExpressionAst);
use_alias(spp::asts, ExpressionAst);
use_alias(spp::asts, IdentifierAst);
use_alias(spp::asts, InnerScopeExpressionAst);
use_alias(spp::asts, LetStatementInitializedAst);
use_alias(spp::asts, PostfixExpressionAst);
use_alias(spp::asts, StatementAst);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::asts::meta, MetaGuard);

auto spp::analyse::utils::expr_utils::ExpandTupleSpread(
  ExpressionAst &tuple, Ast const &tok, ScopeManager *sm, CompilerMetaData *meta)
  -> Opt<TupleSpread> {
  using errors::SppExpansionOfNonTupleError;

  // A value that is no place ("..g()") is evaluated once ("HoistOnce"), so each element does not run it again; a
  // place ("t", "self.items") is read where it is, as a borrowed spread of it ("&..self.items") borrows it.
  // A template's pack is open ("PackInfo"), whatever it declares its elements to be: it stays a spread. An instance's
  // is closed, and is its elements' parameters ("xs#0", "xs#1", ...), each a name of its own. A template's comp pack
  // ("cmp ..n: Bool") is open in the same way; an instance's is bound to its tuple, expanded below.
  if (auto const *const name = tuple.To<asts::IdentifierAst>(); name != nullptr) {
    auto const *const sym = sm->CurrentScope->FindVarSymbol(name);
    if (sym != nullptr and sym->Pack.has_value()) {
      if (sym->Pack->Open) { return Nullopt; }
      auto closed = TupleSpread();
      for (auto const &element : sym->Pack->Elements) {
        closed.Elems.emplace_back(MakeUnique<asts::IdentifierAst>(name->PosStart(), Str(element->Val)));
      }
      return closed;
    }
  }

  auto spread = TupleSpread();
  auto hoisted = HoistOnce(tuple, "$spread", sm, meta);
  spread.Temp = std::move(hoisted.Let);
  auto *const read = hoisted.Name != nullptr ? static_cast<ExpressionAst*>(hoisted.Name.get()) : &tuple;

  // An open tuple (a template's pack) has no length to expand to: each instance, where it is closed, expands it.
  const auto ref = read->InferTypeRef(sm, meta);
  if (type_compare::IsOpenTuple(ref.WithoutConvention(), *sm->CurrentScope)) { return Nullopt; }
  if (not type_predicates::IsTypeTuple(ref.WithoutConvention(), *sm->CurrentScope)) {
    const auto type = read->InferType(sm, meta);
    Raise<SppExpansionOfNonTupleError>(ERR_ARGS(tok, tuple, *type));
  }

  // "t.0", "t.1", ...: each element read through its own access.
  const auto count = ref.Symbol->TypeArgRefs().size();
  for (auto i = 0uz; i < count; ++i) { spread.Elems.emplace_back(ElementAccess(*read, i)); }
  return spread;
}

auto spp::analyse::utils::expr_utils::IsPlace(
  ExpressionAst const &value, Scope const &scope) -> bool {
  return scope.FindVarSymbolOutermost(value).first != nullptr;
}

auto spp::analyse::utils::expr_utils::HoistOnce(
  ExpressionAst const &value, const StrView uid_prefix, ScopeManager *sm, CompilerMetaData *meta, const bool analyse)
  -> Hoisted {
  if (IsPlace(value, *sm->CurrentScope)) { return {}; }
  auto name = MakeShared<asts::IdentifierAst>(value.PosStart(), Str(uid_prefix) + "." + spp::utils::Uid());
  auto var = MakeUnique<asts::LocalVariableSingleIdentifierAst>(nullptr, name, nullptr);
  auto let = MakeUnique<asts::LetStatementInitializedAst>(nullptr, std::move(var), nullptr, nullptr, AstClone(&value));
  if (analyse) { let->Stage7_AnalyseSemantics(sm, meta); }
  return {.Let = std::move(let), .Name = std::move(name)};
}

auto spp::analyse::utils::expr_utils::ElementAccess(
  ExpressionAst const &tuple, const std::size_t index) -> Unique<asts::PostfixExpressionAst> {
  auto field = MakeUnique<asts::IdentifierAst>(tuple.PosStart(), std::to_string(index));
  return MakeUnique<asts::PostfixExpressionAst>(
    AstClone(&tuple), MakeUnique<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field)));
}

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

auto spp::analyse::utils::expr_utils::MarkValueDiscarded(
  Ast &member) -> void {
  // As "DiscardValue" walks in: a bare block's value is
  // its final statement.
  if (auto *const case_expr = member.To<CaseExpressionAst>()) {
    case_expr->ValueDiscarded = true;
    return;
  }
  if (auto *const block = member.To<InnerScopeExpressionAst>(); block != nullptr) {
    block->ValueDiscarded = true;
    if (not block->Members.empty()) { MarkValueDiscarded(*block->FinalMember()); }
  }
}

auto spp::analyse::utils::expr_utils::DiscardValue(
  InnerScopeExpressionAst &owner, const std::size_t i,
  ScopeManager const &sm, CompilerMetaData *const meta) -> void {
  using errors::SppDiscardedValueError;
  auto &member = *owner.Members[i];
  auto *const scope = owner.GetAstScope();
  if (scope == nullptr) { return; }

  // A "case" written as a statement discards whatever its branches
  // end on, each in its own body; a bare block, its final statement.
  if (auto const *const case_expr = member.To<CaseExpressionAst>()) {
    for (auto const &branch : case_expr->Branches) {
      if (branch->Body == nullptr or branch->Body->Members.empty()) { continue; }
      DiscardValue(*branch->Body, branch->Body->Members.size() - 1, sm, meta);
    }
    return;
  }
  if (auto *const block = member.To<InnerScopeExpressionAst>()) {
    if (not block->Members.empty()) { DiscardValue(*block, block->Members.size() - 1, sm, meta); }
    return;
  }

  // Only an expression produces a value; a "let" or an assignment
  // has nothing to discard. Inferred against the scope the
  // statement was written in.
  const auto expr = member.To<ExpressionAst>();
  if (expr == nullptr) { return; }
  auto tm = sm.NewManagerAt(scope);
  const auto type = [&] {
    const auto _meta_guard = MetaGuard(meta);
    meta->IgnoreMissingElseBranchForInference = true;
    return expr->InferType(&tm, meta);
  }();
  if (type == nullptr) { Ice(member, *scope, "discarded expression has no inferred type"); }
  if (type->IsNeverType()) { return; }

  // A value of a "!must_use" type is there to be looked at, and so is
  // a variant that may hold one ("Res" may be a "Fail").
  const auto ref = expr->InferTypeRef(&tm, meta).WithoutConvention();
  const auto must_use = [](TypeRef const &r) { return r.Symbol != nullptr and r.Symbol->IsMustUse(); };
  if (must_use(ref) or (type_predicates::IsTypeVariant(ref, *scope)
    and genex::any_of(type_members::VariantMemberRefs(ref, *scope), must_use))) {
    const auto type_name = type->ToString();
    Raise<SppDiscardedValueError>(ERR_ARGS(member, StrView(type_name)));
  }

  // A temporary nothing takes is dropped once made. A place
  // ("x", "x.a") written as a statement takes nothing from it.
  if (drop_utils::IsTemporary(*expr, tm, meta)) { owner.SetDropsDiscarded(i); }
}
