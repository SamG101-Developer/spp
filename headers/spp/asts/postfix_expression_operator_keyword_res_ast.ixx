module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_keyword_res_ast;
import spp.asts.ast_kind;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(PostfixExpressionOperatorKeywordResAst);
use(spp::asts, struct PostfixExpressionAst);
use(spp::asts, struct FunctionCallArgumentGroupAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorKeywordResAst final : PostfixExpressionOperatorAst {
  SPP_AST_KEY_FUNCTIONS(PostfixExpressionOperatorKeywordResAst);

  /// The "." token that indicates a member access operation.
  Unique<TokenAst> TokDot;

  /// The "res" token that indicates a keyword res operation.
  Unique<TokenAst> TokRes;

  /// The arguments passed to the res keyword. These are passed
  /// into the mapped function for the generator type, to
  /// provide uniform function call analysis.
  Unique<FunctionCallArgumentGroupAst> FnArgGroup;

  PostfixExpressionOperatorKeywordResAst(
    decltype(TokDot) &&tok_dot,
    decltype(TokRes) &&tok_res,
    decltype(FnArgGroup) &&arg_group);

  ~PostfixExpressionOperatorKeywordResAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Unique<PostfixExpressionOperatorAst> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

private:
  Shared<PostfixExpressionAst> _MappedFunc;
};
