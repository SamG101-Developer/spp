module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.postfix_expression_operator_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorRuntimeMemberAccessAst;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst final : PostfixExpressionOperatorAst {
    std::unique_ptr<IdentifierAst> name;

    explicit PostfixExpressionOperatorRuntimeMemberAccessAst(
        decltype(name) name);
    ~PostfixExpressionOperatorRuntimeMemberAccessAst() override;
    auto to_rust() const -> std::string override;
};
