module;
#include <spp/macros.hpp>

export module spp.asts.binary_expression_ast;
import spp.asts.expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct BinaryExpressionAst;
    SPP_EXP_CLS struct TokenAst;
}


/**
 * The BinaryExpressionAst represents a binary expression in the source code, which consists of two operands (left-hand
 * side and right-hand side) and an operator that defines the operation to be performed on those operands. Each operator
 * maps the expressions into a method on the left-hand-side type, so @code 1 + 2@endcode will map to the method
 * @code 1.add(2)@endcode.
 */
SPP_EXP_CLS struct spp::asts::BinaryExpressionAst final : ExpressionAst {
    std::unique_ptr<ExpressionAst> lhs;
    std::unique_ptr<TokenAst> tok_op;
    std::unique_ptr<ExpressionAst> rhs;

    explicit BinaryExpressionAst(
        decltype(lhs) &&lhs,
        decltype(tok_op) &&tok_op,
        decltype(rhs) &&rhs);
    ~BinaryExpressionAst() override;
    auto to_rust() const -> std::string override;
};
