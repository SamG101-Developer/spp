module;
#include <spp/macros.hpp>

export module spp.asts.is_expression_ast;
import spp.asts.expression_ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IsExpressionAst;
    SPP_EXP_CLS struct CaseExpressionAst;
    SPP_EXP_CLS struct CasePatternVariantAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::IsExpressionAst final : ExpressionAst {
private:
    std::shared_ptr<CaseExpressionAst> m_mapped_func;

    std::shared_ptr<IdentifierAst> m_lhs_as_id;

public:
    /**
     * The left-hand side expression of the is expression. This is the first operand.
     */
    std::unique_ptr<ExpressionAst> lhs;

    /**
     * The operator token that represents the is operation. This indicates the type of operation being performed.
     */
    std::unique_ptr<TokenAst> tok_op;

    /**
     * The right-hand side expression of the is expression. This is the second operand.
     */
    std::unique_ptr<CasePatternVariantAst> rhs;

    /**
     * Construct the IsExpressionAst with the arguments matching the members.
     * @param[in] lhs The left-hand side expression of the is expression.
     * @param[in] tok_op The operator token that represents the is operation.
     * @param[in] rhs The right-hand side expression of the is expression.
     */
    IsExpressionAst(
        decltype(lhs) &&lhs,
        decltype(tok_op) &&tok_op,
        decltype(rhs) &&rhs);

    ~IsExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    SPP_ATTR_NODISCARD auto mapped_func() const -> std::shared_ptr<CaseExpressionAst>;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_11_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
