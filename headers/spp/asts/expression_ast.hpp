#ifndef EXPRESSION_AST_HPP
#define EXPRESSION_AST_HPP

#include <spp/asts/statement_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The ExpressionAst class is the base class for all expressions in the abstract syntax tree. It inherits from
 * StatementAst, allowing it to be used in places where a statement is expected, but also providing additional
 * functionality specific to expressions.
 *
 * @n
 * Other base classes inherit this class, such as the PrimaryExpressionAst, which represents the most basic form of
 * expressions. Other expression such as unary, postfix, and binary expressions also inherit this class.
 *
 * @note This class is more of a "marker" to see where expressions should be, as there is no additional functionality in
 * this class.
 */
struct spp::asts::ExpressionAst : StatementAst {
    using StatementAst::StatementAst;
};


#endif //EXPRESSION_AST_HPP
