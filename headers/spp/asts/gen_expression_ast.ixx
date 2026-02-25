module;
#include <spp/macros.hpp>

export module spp.asts.gen_expression_ast;
import spp.asts.primary_expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct GenExpressionAst;
}


/**
 * The GenExpressionAst represents a value being yielded out of a coroutine. A convention can be applied to the value,
 * to create foundational structures like iterators.
 */
SPP_EXP_CLS struct spp::asts::GenExpressionAst final : PrimaryExpressionAst {
    std::unique_ptr<ConventionAst> conv;
    std::unique_ptr<ExpressionAst> expr;

    explicit GenExpressionAst(
        decltype(conv) &&conv,
        decltype(expr) &&expr);
    ~GenExpressionAst() override;
    auto to_rust() const -> std::string override;
};
