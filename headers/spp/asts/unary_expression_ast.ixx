module;
#include <spp/macros.hpp>

export module spp.asts.unary_expression_ast;
import spp.asts.expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct UnaryExpressionAst;
    SPP_EXP_CLS struct UnaryExpressionOperatorAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::UnaryExpressionAst final : ExpressionAst {
    std::unique_ptr<UnaryExpressionOperatorAst> op;
    std::unique_ptr<ExpressionAst> expr;

    explicit UnaryExpressionAst(
        decltype(op) &&tok_op,
        decltype(expr) &&expr);
    ~UnaryExpressionAst() override;
    auto to_rust() const -> std::string override;
};
