#ifndef UNARY_EXPRESSION_AST_HPP
#define UNARY_EXPRESSION_AST_HPP

#include <spp/asts/expression_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::UnaryExpressionAst final : ExpressionAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The operator token that represents the unary operation. This indicates the type of operation being performed.
     */
    std::unique_ptr<UnaryExpressionOperatorAst> op;

    /**
     * The expression that is being operated on by the unary operator.
     */
    std::unique_ptr<ExpressionAst> expr;

    /**
     * Construct the UnaryExpressionAst with the arguments matching the members.
     * @param[in] tok_op The operator token that represents the unary operation.
     * @param[in] expr The expression that is being operated on by the unary operator.
     */
    UnaryExpressionAst(
        decltype(op) &&tok_op,
        decltype(expr) &&expr);

    ~UnaryExpressionAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};


#endif //UNARY_EXPRESSION_AST_HPP
