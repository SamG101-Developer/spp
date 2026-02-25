module;
#include <spp/macros.hpp>

export module spp.asts.gen_with_expression_ast;
import spp.asts.primary_expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenWithExpressionAst;
}


SPP_EXP_CLS struct spp::asts::GenWithExpressionAst final : PrimaryExpressionAst {
    std::unique_ptr<ExpressionAst> expr;

    explicit GenWithExpressionAst(
        decltype(expr) &&expr);
    ~GenWithExpressionAst() override;
    auto to_rust() const -> std::string override;
};
