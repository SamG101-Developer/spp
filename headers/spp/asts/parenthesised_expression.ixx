module;
#include <spp/macros.hpp>

export module spp.asts.parenthesised_expression_ast;
import spp.asts.primary_expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ParenthesisedExpressionAst;
}


SPP_EXP_CLS struct spp::asts::ParenthesisedExpressionAst final : PrimaryExpressionAst {
    std::unique_ptr<ExpressionAst> expr;

    explicit ParenthesisedExpressionAst(
        decltype(expr) &&expr);
    ~ParenthesisedExpressionAst() override;
    auto to_rust() const -> std::string override;
};
