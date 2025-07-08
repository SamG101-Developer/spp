#ifndef LITERAL_AST_HPP
#define LITERAL_AST_HPP

#include <spp/asts/_fwd.hpp>
#include <spp/asts/primary_expression_ast.hpp>


struct spp::asts::LiteralAst : PrimaryExpressionAst {
    using PrimaryExpressionAst::PrimaryExpressionAst;
};


#endif //LITERAL_AST_HPP
