module;
#include <spp/macros.hpp>

export module spp.asts.type_binary_expression_temp_ast;
import spp.asts._fwd;

import std;


SPP_EXP struct spp::asts::TypeBinaryExpressionTempAst final {
    /**
     * The operator token that represents the type binary operation. This indicates the type of operation being
     * performed.
     */
    std::unique_ptr<TokenAst> tok_op;

    /**
     * The right-hand side expression of the type binary expression. This is the second operand.
     */
    std::unique_ptr<TypeAst> rhs;

    /**
     * Construct the TypeBinaryExpressionAst with the arguments matching the members.
     * @param[in] tok_op The operator token that represents the type binary operation.
     * @param[in] rhs The right-hand side expression of the type binary expression.
     */
    TypeBinaryExpressionTempAst(
        decltype(tok_op) &&tok_op,
        decltype(rhs) &&rhs);

    ~TypeBinaryExpressionTempAst();
};
