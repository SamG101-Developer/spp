#ifndef LET_STATEMENT_INITIALIZED_AST_HPP
#define LET_STATEMENT_INITIALIZED_AST_HPP

#include <spp/asts/let_statement_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::LetStatementInitializedAst final : LetStatementAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c let token that starts this statement. It is used to indicate the beginning of a let statement.
     */
    std::unique_ptr<TokenAst> tok_let;

    /**
     * The variable that is being declared in the let statement. This names the symbols that will be created in the
     * scope that the @c let statement is defined in.
     */
    std::unique_ptr<LocalVariableAst> var;

    /**
     * The optionally provided type of the variable. Variable type's can always be inferred from their value, but
     * providing the type allows for variant types to be used with values of an inner type.
     */
    std::unique_ptr<TypeAst> type;

    /**
     * The @c = token that indicates the assignment of a value to the variable. This is used to indicate that the
     * variable is being initialized with a value.
     */
    std::unique_ptr<TokenAst> tok_assign;

    /**
     * The value that is being assigned to the variable. This is the expression that will be evaluated and assigned to
     * the variable.
     */
    std::unique_ptr<ExpressionAst> val;

    /**
     * Construct the LetStatementInitializedAst with the arguments matching the members.
     * @param tok_let The @c let token that starts this statement.
     * @param var The variable that is being declared in the let statement.
     * @param type The optionally provided type of the variable.
     * @param tok_assign The @c = token that indicates the assignment of a value to the variable.
     * @param val The value that is being assigned to the variable.
     */
    LetStatementInitializedAst(
        decltype(tok_let) &&tok_let,
        decltype(var) &&var,
        decltype(type) &&type,
        decltype(tok_assign) &&tok_assign,
        decltype(val) &&val);
};


#endif //LET_STATEMENT_INITIALIZED_AST_HPP
