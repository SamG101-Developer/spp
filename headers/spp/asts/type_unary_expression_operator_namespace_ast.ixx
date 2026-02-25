module;
#include <spp/macros.hpp>

export module spp.asts.type_unary_expression_operator_namespace_ast;
import spp.asts.type_unary_expression_operator_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypeUnaryExpressionOperatorNamespaceAst;
}


SPP_EXP_CLS struct spp::asts::TypeUnaryExpressionOperatorNamespaceAst final : TypeUnaryExpressionOperatorAst {
    std::unique_ptr<IdentifierAst> ns;
    std::unique_ptr<TokenAst> tok_sep;

    explicit TypeUnaryExpressionOperatorNamespaceAst(
        decltype(ns) &&ns,
        decltype(tok_sep) &&tok_sep);
    ~TypeUnaryExpressionOperatorNamespaceAst() override;
    auto to_rust() const -> std::string override;
};
