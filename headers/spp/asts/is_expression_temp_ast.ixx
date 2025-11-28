module;
#include <spp/macros.hpp>

export module spp.asts.is_expression_temp_ast;
import spp.asts._fwd;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct IsExpressionTempAst;
}


SPP_EXP_CLS struct spp::asts::IsExpressionTempAst {
    /**
     * The operator token that represents the is operation. This indicates the type of operation being performed.
     */
    std::unique_ptr<TokenAst> tok_op;

    /**
     * The right-hand side expression of the is expression. This is the second operand.
     */
    std::unique_ptr<CasePatternVariantAst> rhs;

    /**
     * Construct the IsExpressionTempAst with the arguments matching the members.
     * @param[in] tok_op The operator token that represents the is operation.
     * @param[in] rhs The right-hand side expression of the is expression.
     */
    IsExpressionTempAst(
        decltype(tok_op) &&tok_op,
        decltype(rhs) &&rhs);

    ~IsExpressionTempAst();
};
