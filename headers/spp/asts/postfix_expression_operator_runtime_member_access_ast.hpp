#pragma once
#include <spp/asts/postfix_expression_operator_ast.hpp>


struct spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst final : PostfixExpressionOperatorAst {
    /**
     * The @c . token that indicates a runtime member access operation in a postfix expression.
     */
    std::unique_ptr<TokenAst> tok_dot;

    /**
     * The identifier that represents the member being accessed. This is the name of the member in the class or struct.
     */
    std::shared_ptr<IdentifierAst> name;

    /**
     * Construct the PostfixExpressionOperatorMemberAccessAst with the arguments matching the members.
     * @param[in] tok_dot The @c . token that indicates a runtime member access operation in a postfix expression.
     * @param[in] name The identifier that represents the member being accessed.
     */
    explicit PostfixExpressionOperatorRuntimeMemberAccessAst(
        decltype(tok_dot)&& tok_dot,
        decltype(name) name);

    ~PostfixExpressionOperatorRuntimeMemberAccessAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
