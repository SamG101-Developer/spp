module;
#include <spp/macros.hpp>

export module spp.asts.type_parenthesised_expression_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TypeParenthesisedExpressionAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::TypeParenthesisedExpressionAst final : Ast {
    std::unique_ptr<TypeAst> expr;

    explicit TypeParenthesisedExpressionAst(
        decltype(expr) &&expr);
    ~TypeParenthesisedExpressionAst() override;
    auto to_rust() const -> std::string override;
};
