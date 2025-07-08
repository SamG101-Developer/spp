#ifndef STATEMENT_AST_HPP
#define STATEMENT_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The StatementAst class is the base class for all statements in the abstract syntax tree. It is used to represent
 * statements that do not return a value, such as variable declarations and control flow statements.
 */
struct spp::asts::StatementAst : Ast {
    using Ast::Ast;
};


#endif //STATEMENT_AST_HPP
