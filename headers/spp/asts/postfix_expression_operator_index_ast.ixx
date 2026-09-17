module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_index_ast;
import spp.asts.ast_kind;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(PostfixExpressionOperatorIndexAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct PostfixExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, struct TypeRef);

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorIndexAst final : PostfixExpressionOperatorAst {
  SPP_AST_KEY_FUNCTIONS(PostfixExpressionOperatorIndexAst);

  /// The "[" token that starts the index expression.
  Unique<TokenAst> TokL;

  /// The optional "mut" token, making the index mutable.
  Unique<TokenAst> TokMut;

  /// The expression used to index into the lhs expression.
  Unique<ExpressionAst> Expr;

  /// The "]" token that ends the index expression.
  Unique<TokenAst> TokR;

  PostfixExpressionOperatorIndexAst(
    decltype(TokL) &&tok_l,
    decltype(TokMut) &&tok_mut,
    decltype(Expr) &&expr,
    decltype(TokR) &&tok_r);

  ~PostfixExpressionOperatorIndexAst() override;

  /// The left-hand-side must be "indexable", in the same way
  /// that an iterator-based loop's condition must be
  /// "iterable". Either "IterRef" or "IterMut" must be
  /// superimposed over the left-hand-side, depending on the
  /// presence of the "mut" keyword.
  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  /// Infer the type from the mapped "index" function on the
  /// left-hand-side type. For example, "hello"[5] becomes
  /// "hello".index_ref(5), and its return type is used.
  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Unique<PostfixExpressionOperatorAst> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

private:
  Shared<PostfixExpressionAst> _MappedFunc;
};
