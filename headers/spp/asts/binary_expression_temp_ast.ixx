module;
#include <spp/macros.hpp>

export module spp.asts.binary_expression_temp_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct BinaryExpressionTempAst;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct TokenAst;
}


/**
 * Temporary AST node that hold the right-hand-side of a binary expression. This is because it is parsed together, aside
 * from the left-hand-side, and as an entirety may be nullptr, and discarded.
 */
SPP_EXP_CLS struct spp::asts::BinaryExpressionTempAst {
    std::unique_ptr<TokenAst> tok_op;
    std::unique_ptr<ExpressionAst> rhs;

    explicit BinaryExpressionTempAst(
        decltype(tok_op) &&tok_op,
        decltype(rhs) &&rhs);
    ~BinaryExpressionTempAst();
};
