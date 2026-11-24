module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.pattern_guard_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.common_errors;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.type_predicates;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, VariableSymbol);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::PatternGuardAst::PatternGuardAst(
  decltype(TokAnd) &&tok_and,
  decltype(Expr) &&expression) :
  TokAnd(std::move(tok_and)),
  Expr(std::move(expression)) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAnd, lex::SppTokenType::KW_AND, "and", Expr ? Expr->PosStart() : 0);
}

spp::asts::PatternGuardAst::~PatternGuardAst() = default;

auto spp::asts::PatternGuardAst::PosStart() const -> std::size_t {
  // Use the "and" token.
  return TokAnd->PosStart();
}

auto spp::asts::PatternGuardAst::PosEnd() const -> std::size_t {
  // Use the expression.
  return Expr->PosEnd();
}

auto spp::asts::PatternGuardAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<PatternGuardAst>(
    AstClone(TokAnd),
    AstClone(Expr));
}

auto spp::asts::PatternGuardAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokAnd);
  SPP_STRING_APPEND(Expr);
  SPP_STRING_END;
}

auto spp::asts::PatternGuardAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // Check the expression in the pattern guard.
  Expr->Stage7_AnalyseSemantics(sm, meta);
  common_errors::RequirePrimary(*Expr, *sm);

  // Check the guard's type is boolean.
  if (not type_predicates::IsTypeBool(Expr->InferTypeRef(sm, meta), *sm->CurrentScope)) {
    const auto expr_ty = Expr->InferType(sm, meta);
    Raise<SppExpressionNotBooleanError>(ERR_ARGS(*Expr, *expr_ty, "pattern guard"));
  }
}

auto spp::asts::PatternGuardAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // A guard may move a value: it is moved on every path past the
  // branch too, the "case" dropping it where the guard never ran
  // ("case_utils::CarryGuardMoves"). Not a binding of the branch's
  // patterns, or of the "case" condition's "is", whole or any part
  // of it: until the branch is taken it is still a part of the
  // subject, which every later branch matches against whole. A
  // borrow binding takes nothing.
  const auto before = memory_state::SnapshotFlow(*sm->CurrentScope);
  Expr->Stage8_CheckMemory(sm, meta);
  mem_utils::ValidateSymbolMemory(*Expr, *this, *sm, meta, {.CheckMoveFromBorrowedCtx = false, .MarkMoves = false});
  auto bindings = sm->CurrentScope->GetOwners();
  if (sm->CurrentScope->Parent != nullptr) { bindings.append_range(sm->CurrentScope->Parent->GetOwners()); }
  for (const auto sym : bindings) {
    RaiseIf<SppPatternGuardMovesValueError>(
      not sym->IsBorrow() and sym->Name != nullptr
      and not memory_state::SameShape(
        memory_state::StateIn(before, *sym), memory_state::StateOf(*sm->CurrentScope, *sym)),
      ERR_ARGS(*Expr, *sym->Name));
  }

  // Nor the subject itself, or any part of it: a comparison
  // pattern after this branch reads it without a check of its own.
  if (const auto subject = meta->CaseSubject; subject != nullptr and subject->Name != nullptr) {
    RaiseIf<SppPatternGuardMovesValueError>(
      not memory_state::SameShape(
        memory_state::StateIn(before, *subject), memory_state::StateOf(*sm->CurrentScope, *subject)),
      ERR_ARGS(*Expr, *subject->Name));
  }
}

auto spp::asts::PatternGuardAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Generate the expression.
  return Expr->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::asts::PatternGuardAst::EvaluateCompTime(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Resolve the expression at compile-time.
  Expr->EvaluateCompTime(sm, meta);
}

SPP_MOD_END
