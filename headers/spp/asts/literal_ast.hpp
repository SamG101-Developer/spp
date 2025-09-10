#pragma once
#include <spp/asts/primary_expression_ast.hpp>


struct spp::asts::LiteralAst : PrimaryExpressionAst {
    using PrimaryExpressionAst::PrimaryExpressionAst;
};
