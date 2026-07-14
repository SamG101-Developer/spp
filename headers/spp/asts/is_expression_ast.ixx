module;
#include <spp/macros.hpp>

export module spp.asts.is_expression_ast;
import spp.asts.expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IsExpressionAst;
    SPP_EXP_CLS struct CaseExpressionAst;
    SPP_EXP_CLS struct CasePatternVariantAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::IsExpressionAst final : ExpressionAst {
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

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;

    SPP_ATTR_NODISCARD auto GetMappedFunc() const -> CaseExpressionAst*;

private:
    Unique<CaseExpressionAst> _MappedFunc;
    Unique<IdentifierAst> _LhsAsId;
};
