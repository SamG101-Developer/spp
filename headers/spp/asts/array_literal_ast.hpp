#ifndef ARRAY_LITERAL_AST_HPP
#define ARRAY_LITERAL_AST_HPP

#include <spp/asts/literal_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The ArrayLiteralAst is the base class for the two array literals - ArrayLiteral0Elements and ArrayLiteralNElements.
 * The common base class is for type checking only.
 */
struct spp::asts::ArrayLiteralAst : LiteralAst {
    using LiteralAst::LiteralAst;
};


#endif //ARRAY_LITERAL_AST_HPP
