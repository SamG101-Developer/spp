module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.postfix_expression_operator_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorStaticMemberAccessAst;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorStaticMemberAccessAst final : PostfixExpressionOperatorAst {
    std::unique_ptr<IdentifierAst> name;

    explicit PostfixExpressionOperatorStaticMemberAccessAst(
        decltype(name) &&name);
    ~PostfixExpressionOperatorStaticMemberAccessAst() override;
    auto to_rust() const -> std::string override;
};
