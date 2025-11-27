module;
#include <spp/macros.hpp>

export module spp.asts.literal_ast;
import spp.asts.primary_expression_ast;


SPP_EXP_CLS struct spp::asts::LiteralAst : PrimaryExpressionAst {
    using PrimaryExpressionAst::PrimaryExpressionAst;

    ~LiteralAst() override;
};
