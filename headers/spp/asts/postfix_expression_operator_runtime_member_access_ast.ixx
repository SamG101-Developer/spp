module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts._fwd;
import spp.asts.postfix_expression_operator_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct PostfixExpressionOperatorRuntimeMemberAccessAst;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst final : PostfixExpressionOperatorAst {
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
        decltype(tok_dot) &&tok_dot,
        decltype(name) name);

    ~PostfixExpressionOperatorRuntimeMemberAccessAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
