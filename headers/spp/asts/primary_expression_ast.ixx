module;
#include <spp/macros.hpp>

export module spp.asts.primary_expression_ast;
import spp.asts.expression_ast;


SPP_EXP_CLS struct spp::asts::PrimaryExpressionAst : ExpressionAst {
    using ExpressionAst::ExpressionAst;

    ~PrimaryExpressionAst() override;
};
