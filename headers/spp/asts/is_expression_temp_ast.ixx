module;
#include <spp/macros.hpp>

export module spp.asts.is_expression_temp_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantAst;
    SPP_EXP_CLS struct IsExpressionTempAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::IsExpressionTempAst {
    std::unique_ptr<TokenAst> tok_op;
    std::unique_ptr<CasePatternVariantAst> rhs;

    IsExpressionTempAst(
        decltype(tok_op) &&tok_op,
        decltype(rhs) &&rhs);
    ~IsExpressionTempAst();
};
