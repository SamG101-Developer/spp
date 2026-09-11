module;
#include <spp/macros.hpp>

export module spp.asts.unary_expression_ast;
import spp.asts.ast_kind;
import spp.asts.expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(UnaryExpressionAst) {
  SPP_EXP_CLS struct GenericArgumentAst;
  SPP_EXP_CLS struct UnaryExpressionOperatorAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::UnaryExpressionAst final : ExpressionAst {
  SPP_AST_KEY_FUNCTIONS(UnaryExpressionAst);

  /**
   * The operator token that represents the unary operation. This indicates the type of operation being performed.
   */
  Unique<UnaryExpressionOperatorAst> Op;

  /**
   * The expression that is being operated on by the unary operator.
   */
  Unique<ExpressionAst> Expr;

  /**
   * Construct the UnaryExpressionAst with the arguments matching the members.
   * @param[in] tok_op The operator token that represents the unary operation.
   * @param[in] expr The expression that is being operated on by the unary operator.
   */
  UnaryExpressionAst(
    decltype(Op) &&tok_op,
    decltype(Expr) &&expr);

  ~UnaryExpressionAst() override;

  auto Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* override;

  auto InferType(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Shared<ExpressionAst> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const
    -> bool override;
};
