module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.pattern_guard_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.type_predicates;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;

SPP_MOD_BEGIN
PatternGuardAst::PatternGuardAst(
  decltype(TokAnd) &&tok_and,
  decltype(Expr) &&expression) :
  TokAnd(std::move(tok_and)),
  Expr(std::move(expression)) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAnd, lex::SppTokenType::KW_AND, "and", Expr ? Expr->PosStart() : 0);
}

PatternGuardAst::~PatternGuardAst() = default;

auto PatternGuardAst::PosStart() const -> std::size_t {
  // Use the "and" token.
  return TokAnd->PosStart();
}

auto PatternGuardAst::PosEnd() const -> std::size_t {
  // Use the expression.
  return Expr->PosEnd();
}

auto PatternGuardAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<PatternGuardAst>(
    AstClone(TokAnd),
    AstClone(Expr));
}

auto PatternGuardAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokAnd);
  SPP_STRING_APPEND(Expr);
  SPP_STRING_END;
}

auto PatternGuardAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // Check the expression in the pattern guard.
  Expr->Stage7_AnalyseSemantics(sm, meta);
  RaiseIf<SppInvalidPrimaryExpressionError>(
    not expr_utils::IsPrimaryExprTypeValid(*Expr, *sm),
    {sm->CurrentScope}, ERR_ARGS(*Expr.get()));

  // Check the guard's type is boolean.
  if (not type_predicates::IsTypeBool(Expr->InferTypeRef(sm, meta), *sm->CurrentScope)) {
    const auto expr_ty = Expr->InferType(sm, meta);
    Raise<SppExpressionNotBooleanError>({sm->CurrentScope}, ERR_ARGS(*Expr, *expr_ty, "pattern guard"));
  }
}

auto PatternGuardAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // Check the memory of the expression. A guard may read and
  // borrow, but not move: it runs before its branch is chosen,
  // so a move it makes would also have happened on the path
  // into the next branch when it answers false.
  const auto before = sm->CurrentScope->AllVarSymbols()
    | genex::views::transform([](auto *sym) { return MakePair(sym, spp::get<0>(sym->MemInfo->AstMoved)); })
    | genex::to<Vec>();
  Expr->Stage8_CheckMemory(sm, meta);
  mem_utils::ValidateSymbolMemory(*Expr, *this, *sm, true, true, false, false, meta);
  for (auto const &[sym, moved_before] : before) {
    RaiseIf<SppPatternGuardMovesValueError>(
      moved_before == nullptr and spp::get<0>(sym->MemInfo->AstMoved) != nullptr and sym->Name != nullptr,
      {sm->CurrentScope}, ERR_ARGS(*Expr, *sym->Name));
  }
}

auto PatternGuardAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Resolve the expression at compile-time.
  Expr->Stage9_CompTimeResolve(sm, meta);
}

auto PatternGuardAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Generate the expression.
  return Expr->Stage11_CodeGen(sm, meta, ctx);
}

SPP_MOD_END
