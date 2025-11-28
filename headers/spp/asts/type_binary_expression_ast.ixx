module;
#include <spp/macros.hpp>

export module spp.asts.type_binary_expression_ast;
import spp.asts.ast;
import spp.asts.mixins.temp_type_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct TypeBinaryExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::TypeBinaryExpressionAst final : virtual Ast, mixins::TempTypeAst {
    /**
     * The left-hand side expression of the type binary expression. This is the first operand.
     */
    std::shared_ptr<TypeAst> lhs;

    /**
     * The operator token that represents the type binary operation. This indicates the type of operation being
     * performed. Either an "or" (union) or "and" (intersection) operation.
     */
    std::unique_ptr<TokenAst> tok_op;

    /**
     * The right-hand side expression of the type binary expression. This is the second operand.
     */
    std::shared_ptr<TypeAst> rhs;

    /**
     * Construct the TypeBinaryExpressionAst with the arguments matching the members.
     * @param lhs The left-hand side expression of the type binary expression.
     * @param tok_op The operator token that represents the type binary operation.
     * @param rhs The right-hand side expression of the type binary expression.
     */
    TypeBinaryExpressionAst(
        decltype(lhs) &&lhs,
        decltype(tok_op) &&tok_op,
        decltype(rhs) &&rhs);

    ~TypeBinaryExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto convert() -> std::unique_ptr<TypeAst> override;
};
