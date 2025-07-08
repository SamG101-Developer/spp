#ifndef ASSIGNMENT_STATEMENT_AST_HPP
#define ASSIGNMENT_STATEMENT_AST_HPP

#include <spp/asts/statement_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * An assignment statement allows for one value to be moved or copied into another variable or property (or multiple
 * values into multiple variables or properties at once). Only symbolic variables and properties can be assigned to.
 *
 * @note Compound assignment expressions, such as @c += or @c -=, are classified under @c BinaryExpressionAst nodes,
 * because they share function mapping logic with binary expressions, and can only have 1 left-hand-side expression
 * anyway. Some checks are copied from this class (symbolic target, etc.).
 */
struct spp::asts::AssignmentStatementAst final : StatementAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The list of left-hand side expressions in the assignment statement. These are the variables or properties that
     * are being assigned a value.
     */
    std::vector<std::unique_ptr<ExpressionAst>> lhs;

    /**
     * The @c = token that represents the assignment operator. This indicates to the parser that an assignment statement
     * has been defined.
     */
    std::unique_ptr<TokenAst> tok_assign;

    /**
     * The list of right-hand side expressions in the assignment statement. These are the values or expressions that
     * are being assigned to the left-hand side variables or properties.
     */
    std::vector<std::unique_ptr<ExpressionAst>> rhs;

    /**
     * Construct the AssignmentStatementAst with the arguments matching the members.
     * @param[in] pos The position of the AST in the source code.
     * @param[in] lhs The list of left-hand side expressions in the assignment statement.
     * @param[in] tok_assign The @c = token that represents the assignment operator.
     * @param[in] rhs The list of right-hand side expressions in the assignment statement.
     */
    AssignmentStatementAst(
        decltype(lhs) &&lhs,
        decltype(tok_assign) &&tok_assign,
        decltype(rhs) &&rhs);
};


#endif //ASSIGNMENT_STATEMENT_AST_HPP
