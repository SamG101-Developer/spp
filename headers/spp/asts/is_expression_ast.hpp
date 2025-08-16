#ifndef IS_EXPRESSION_AST_HPP
#define IS_EXPRESSION_AST_HPP

#include <spp/asts/expression_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::IsExpressionAst final : ExpressionAst {
    SPP_AST_KEY_FUNCTIONS;

private:
    std::unique_ptr<PostfixExpressionAst> m_mapped_func;

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

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};


#endif //IS_EXPRESSION_AST_HPP
