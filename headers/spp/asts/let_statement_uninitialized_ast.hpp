#ifndef LET_STATEMENT_UNINITIALIZED_AST_HPP
#define LET_STATEMENT_UNINITIALIZED_AST_HPP

#include <spp/asts/let_statement_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::LetStatementUninitializedAst final : LetStatementAst {
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
     * The @c : token that indicates the type of the variable. This separates the variable name from its type in the
     * @c let statement.
     */
    std::unique_ptr<TokenAst> tok_colon;

    /**
     * The type of the uninitialized variable. This is used to check that values being assigned to the variable in the
     * future are of the correct type.
     */
    std::unique_ptr<TypeAst> type;

    /**
     * Construct the LetStatementUninitializedAst with the arguments matching the members.
     * @param[in] tok_let The @c let token that starts this statement.
     * @param[in] var The variable that is being declared in the let statement.
     * @param[in] tok_colon The @c : token that indicates the type of the variable.
     * @param[in] type The type of the uninitialized variable.
     */
    LetStatementUninitializedAst(
        decltype(tok_let) &&tok_let,
        decltype(var) &&var,
        decltype(tok_colon) &&tok_colon,
        decltype(type) &&type);
};


#endif //LET_STATEMENT_UNINITIALIZED_AST_HPP
