module;
#include <spp/macros.hpp>

export module spp.asts.type_postfix_expression_operator_nested_type_ast;
import spp.asts.type_postfix_expression_operator_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypePostfixExpressionOperatorNestedTypeAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
}


SPP_EXP_CLS struct spp::asts::TypePostfixExpressionOperatorNestedTypeAst final : TypePostfixExpressionOperatorAst {
    std::unique_ptr<TokenAst> tok_sep;
    std::unique_ptr<TypeIdentifierAst> name;

    explicit TypePostfixExpressionOperatorNestedTypeAst(
        decltype(tok_sep) &&tok_sep,
        decltype(name) &&name);
    ~TypePostfixExpressionOperatorNestedTypeAst() override;
    auto to_rust() const -> std::string override;
};
