module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_index_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.operator_desugaring;
import spp.analyse.utils.type_compare;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.ptr;
import genex;

use_alias(spp::analyse::scopes, ExprSubst);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorIndexAst::PostfixExpressionOperatorIndexAst(
  Unique<TokenAst> &&tok_l,
  Unique<TokenAst> &&tok_mut,
  Unique<ExpressionAst> &&expr,
  Unique<TokenAst> &&tok_r) :
  TokL(std::move(tok_l)),
  TokMut(std::move(tok_mut)),
  Expr(std::move(expr)),
  TokR(std::move(tok_r)),
  _MappedFn(nullptr) {
}

spp::asts::PostfixExpressionOperatorIndexAst::~PostfixExpressionOperatorIndexAst() = default;

auto spp::asts::PostfixExpressionOperatorIndexAst::PosStart() const -> std::size_t {
  // Use the "[" token.
  return TokL->PosStart();
}

auto spp::asts::PostfixExpressionOperatorIndexAst::PosEnd() const -> std::size_t {
  // Use the "]" token.
  return TokR->PosEnd();
}

auto spp::asts::PostfixExpressionOperatorIndexAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<PostfixExpressionOperatorIndexAst>(
    AstClone(TokL),
    AstClone(TokMut),
    AstClone(Expr),
    AstClone(TokR));
  ast->_MappedFn = _MappedFn;
  return ast;
}

auto spp::asts::PostfixExpressionOperatorIndexAst::ToString() const -> Str {
  SPP_STRING_START;
  if (_MappedFn != nullptr) {
    SPP_STRING_APPEND(_MappedFn->Op);
    SPP_STRING_END;
  }
  SPP_STRING_APPEND_RAW("[");
  SPP_STRING_APPEND(TokMut).append(TokMut ? " " : "");
  SPP_STRING_APPEND(Expr);
  SPP_STRING_APPEND_RAW("]");
  SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorIndexAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Already analysed => return early.
  IMPORT_UTILS;
  if (_MappedFn != nullptr) { return; }

  // "a[i]" is "a.index_ref(i)", or "a.index_mut(i)" for "a[mut i]".
  operator_desugaring::CheckIndexable(*this, *Lhs, sm, meta);
  auto args = Vec<Unique<ExpressionAst>>();
  args.emplace_back(std::move(Expr));
  _MappedFn = operator_desugaring::MapToMethodCall(
    *this, *Lhs, PosStart(), TokMut != nullptr ? "index_mut" : "index_ref", std::move(args), sm, meta);
}

auto spp::asts::PostfixExpressionOperatorIndexAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  _MappedFn->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorIndexAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Forward to the mapped function.
  return _MappedFn->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::asts::PostfixExpressionOperatorIndexAst::EvaluateCompTime(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward to the mapped function.
  _MappedFn->EvaluateCompTime(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorIndexAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // Forward to the mapped function's return type.
  return _MappedFn->InferType(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorIndexAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  return _MappedFn->InferTypeRef(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorIndexAst::ReadExpr(
  analyse::scopes::ExprSubst const &sub) const -> Unique<PostfixExpressionOperatorAst> {
  // Substitute the inner expression inside the []
  // tokens.
  return MakeUnique<PostfixExpressionOperatorIndexAst>(
    AstClone(TokL),
    AstClone(TokMut),
    AstClone(Expr->ReadExpr(sub)),
    AstClone(TokR));
}

auto spp::asts::PostfixExpressionOperatorIndexAst::IsAllowedInDefault() const -> bool {
  // Check the inner expression.
  return Expr->IsAllowedInDefault();
}

auto spp::asts::PostfixExpressionOperatorIndexAst::WalksOperand() const -> bool {
  // Checked through "index_ref"/"index_mut", over the operand.
  return true;
}

SPP_MOD_END
