module;
#include <spp/macros.hpp>

export module spp.asts.parenthesised_expression_ast;
import spp.asts.ast_kind;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(ParenthesisedExpressionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct TokenAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::ParenthesisedExpressionAst final : PrimaryExpressionAst {
  SPP_AST_KEY_FUNCTIONS(ParenthesisedExpressionAst);

  /// The "(" token starting the parenthesised expression.
  Unique<TokenAst> TokL;

  /// The expression enclosed in parentheses.
  Unique<ExpressionAst> Expr;

  /// The ")" token ending the parenthesised expression.
  Unique<TokenAst> TokR;

  explicit ParenthesisedExpressionAst(
    decltype(TokL) &&tok_open_paren,
    decltype(Expr) &&expr,
    decltype(TokR) &&tok_close_paren);

  ~ParenthesisedExpressionAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Shared<ExpressionAst> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;
};
