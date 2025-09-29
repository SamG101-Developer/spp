#pragma once
#include <spp/asts/postfix_expression_operator_ast.hpp>


struct spp::asts::PostfixExpressionOperatorKeywordResAst final : PostfixExpressionOperatorAst {
    SPP_AST_KEY_FUNCTIONS;

private:
    std::shared_ptr<PostfixExpressionAst> m_mapped_func;

public:
    /**
     * The @c . token that indicates a member access operation.
     */
    std::unique_ptr<TokenAst> tok_dot;

    /**
     * The @c res token that indicates a keyword res operation.
     */
    std::unique_ptr<TokenAst> tok_res;

    /**
     * The arguments passed to the res keyword. These will be passed into the mapped function for the generator type, to
     * provide uniform function call analysis.
     */
    std::unique_ptr<FunctionCallArgumentGroupAst> arg_group;

    /**
     * Construct the PostfixExpressionOperatorKeywordResAst with the arguments matching the members.
     * @param tok_dot The @c . token that indicates a member access operation.
     * @param tok_res The @c res token that indicates a keyword res operation.
     * @param arg_group The arguments passed to the res keyword.
     */
    PostfixExpressionOperatorKeywordResAst(
        decltype(tok_dot) &&tok_dot,
        decltype(tok_res) &&tok_res,
        decltype(arg_group) &&arg_group);

    ~PostfixExpressionOperatorKeywordResAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
