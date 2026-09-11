module;
#include <spp/macros.hpp>

export module spp.asts.is_expression_ast;
import spp.asts.ast_kind;
import spp.asts.expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(IsExpressionAst) {
  SPP_EXP_CLS struct CaseExpressionAst;
  SPP_EXP_CLS struct CasePatternVariantAst;
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::IsExpressionAst final : ExpressionAst {
  SPP_AST_KEY_FUNCTIONS(IsExpressionAst);

  /**
   * The left-hand side expression of the is expression. This is the first operand.
   */
  Unique<ExpressionAst> Lhs;

  /**
   * The operator token that represents the is operation. This indicates the type of operation being performed.
   */
  Unique<TokenAst> TokOp;

  /**
   * The right-hand side expression of the is expression. This is the second operand.
   */
  Unique<CasePatternVariantAst> Rhs;

  struct {
    std::size_t OriginalPosStart;
    std::size_t OriginalPosEnd;
  } Source;

  /**
   * Construct the IsExpressionAst with the arguments matching the members.
   * @param[in] lhs The left-hand side expression of the is expression.
   * @param[in] tok_op The operator token that represents the is operation.
   * @param[in] rhs The right-hand side expression of the is expression.
   */
  IsExpressionAst(
    decltype(Lhs) &&lhs,
    decltype(TokOp) &&tok_op,
    decltype(Rhs) &&rhs);

  ~IsExpressionAst() override;

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

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const
    -> bool override;

private:
  Shared<CaseExpressionAst> _MappedFunc;

  Shared<IdentifierAst> _LhsAsId;
};
