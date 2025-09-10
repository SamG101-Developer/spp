#pragma once
#include <spp/asts/postfix_expression_operator_ast.hpp>


struct spp::asts::PostfixExpressionOperatorStaticMemberAccessAst final : PostfixExpressionOperatorAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c :: token that indicates a static member access operation in a postfix expression.
     */
    std::unique_ptr<TokenAst> tok_dbl_colon;

    /**
     * The identifier that represents the member being accessed. This is the name of the member in the class or struct.
     */
    std::shared_ptr<IdentifierAst> name;

    /**
     * Construct the PostfixExpressionOperatorMemberAccessAst with the arguments matching the members.
     * @param[in] tok_dbl_colon The @c :: token that indicates a static member access operation in a postfix expression.
     * @param[in] name The identifier that represents the member being accessed.
     */
    explicit PostfixExpressionOperatorStaticMemberAccessAst(
        decltype(tok_dbl_colon) &&tok_dbl_colon,
        decltype(name) &&name);

    ~PostfixExpressionOperatorStaticMemberAccessAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
