module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_slice_ast;
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

SPP_MOD_BEGIN
PostfixExpressionOperatorSliceAst::PostfixExpressionOperatorSliceAst(
  Unique<TokenAst> &&tok_l,
  Unique<TokenAst> &&tok_mut,
  Unique<ExpressionAst> &&expr_l_bound,
  Unique<TokenAst> &&tok_to,
  Unique<ExpressionAst> &&expr_r_bound,
  Unique<TokenAst> &&tok_r) :
  TokL(std::move(tok_l)),
  TokMut(std::move(tok_mut)),
  ExprLBound(std::move(expr_l_bound)),
  TokTo(std::move(tok_to)),
  ExprRBound(std::move(expr_r_bound)),
  TokR(std::move(tok_r)),
  _MappedFn(nullptr) {
}

PostfixExpressionOperatorSliceAst::~PostfixExpressionOperatorSliceAst() = default;

auto PostfixExpressionOperatorSliceAst::PosStart() const -> std::size_t {
  // Use the "[" token.
  return TokL->PosStart();
}

auto PostfixExpressionOperatorSliceAst::PosEnd() const -> std::size_t {
  // Use the "]" token.
  return TokR->PosEnd();
}

auto PostfixExpressionOperatorSliceAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<PostfixExpressionOperatorSliceAst>(
    AstClone(TokL),
    AstClone(TokMut),
    AstClone(ExprLBound),
    AstClone(TokTo),
    AstClone(ExprRBound),
    AstClone(TokR));
  ast->_MappedFn = _MappedFn;
  return ast;
}

auto PostfixExpressionOperatorSliceAst::ToString() const -> Str {
  SPP_STRING_START;
  if (_MappedFn != nullptr) {
    SPP_STRING_APPEND(_MappedFn->Op);
    SPP_STRING_END;
  }
  SPP_STRING_APPEND_RAW("[");
  SPP_STRING_APPEND(TokMut).append(TokMut ? " " : "");
  SPP_STRING_APPEND(ExprLBound);
  SPP_STRING_APPEND_RAW("to ");
  SPP_STRING_APPEND(ExprRBound);
  SPP_STRING_APPEND_RAW("]");
  SPP_STRING_END;
}

auto PostfixExpressionOperatorSliceAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Already analysed => return early.
  IMPORT_UTILS;
  if (_MappedFn != nullptr) { return; }

  operator_desugaring::CheckIndexable(*this, sm, meta);

  // Decide the function variation based upon if the lhs/rhs are nullptr or not.
  const auto prefix = ExprLBound == nullptr and ExprRBound == nullptr
    ? "full"
    : ExprLBound == nullptr
    ? "to"
    : ExprRBound == nullptr
    ? "from"
    : "";

  const auto mut = TokMut != nullptr ? "mut" : "ref";
  const auto func_name = Str("slice_") + mut + (std::strlen(prefix) == 0 ? "" : "_") + prefix;

  // The bounds written, in order, are the method's arguments.
  auto args = Vec<Unique<ExpressionAst>>();
  if (ExprLBound != nullptr) { args.EmplaceBack(std::move(ExprLBound)); }
  if (ExprRBound != nullptr) { args.EmplaceBack(std::move(ExprRBound)); }
  _MappedFn = operator_desugaring::MapToMethodCall(*this, PosStart(), func_name, std::move(args), sm, meta);
}

auto PostfixExpressionOperatorSliceAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  _MappedFn->Stage8_CheckMemory(sm, meta);
}

auto PostfixExpressionOperatorSliceAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward to the mapped function.
  _MappedFn->Stage9_CompTimeResolve(sm, meta);
}

auto PostfixExpressionOperatorSliceAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Forward to the mapped function.
  return _MappedFn->Stage11_CodeGen(sm, meta, ctx);
}

auto PostfixExpressionOperatorSliceAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // Forward to the mapped function's return type.
  return _MappedFn->InferType(sm, meta);
}

auto PostfixExpressionOperatorSliceAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  return _MappedFn->InferTypeRef(sm, meta);
}

auto PostfixExpressionOperatorSliceAst::ReadExpr(
  analyse::scopes::ExprSubst const &sub) const -> Unique<PostfixExpressionOperatorAst> {
  // Substitute the inner expressions inside the []
  // tokens.
  return MakeUnique<PostfixExpressionOperatorSliceAst>(
    AstClone(TokL),
    AstClone(TokMut),
    AstClone(ExprLBound->ReadExpr(sub)),
    AstClone(TokTo),
    AstClone(ExprRBound->ReadExpr(sub)),
    AstClone(TokR));
}

auto PostfixExpressionOperatorSliceAst::IsAllowedInDefault() const -> bool {
  // Check both the left and right bounds, which can be
  // nullptr for the unbound slicing.
  return
    (ExprLBound == nullptr or ExprLBound->IsAllowedInDefault()) and
    (ExprRBound == nullptr or ExprRBound->IsAllowedInDefault());
}

SPP_MOD_END
