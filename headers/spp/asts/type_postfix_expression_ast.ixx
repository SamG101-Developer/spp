module;
#include <spp/macros.hpp>

export module spp.asts.type_postfix_expression_ast;
import spp.asts.type_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TypePostfixExpressionAst;
    SPP_EXP_CLS struct TypePostfixExpressionOperatorAst;
}


SPP_EXP_CLS struct spp::asts::TypePostfixExpressionAst final : TypeAst {
    std::unique_ptr<TypeAst> lhs;
    std::unique_ptr<TypePostfixExpressionOperatorAst> tok_op;

    explicit TypePostfixExpressionAst(
        decltype(lhs) &&lhs,
        decltype(tok_op) &&tok_op);
    ~TypePostfixExpressionAst() override;
    auto to_rust() const -> std::string override;
};
