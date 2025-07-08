#ifndef BOOLEAN_LITERAL_AST_HPP
#define BOOLEAN_LITERAL_AST_HPP

#include <spp/asts/literal_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The BooleanLiteralAst represents a boolean literal in the source code, which can be either @c true or @c false.
 * This AST is used to represent boolean values in expressions and statements.
 */
struct spp::asts::BooleanLiteralAst final : LiteralAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The token that represents the boolean literal, either @c true or @c false.
     */
    std::unique_ptr<TokenAst> tok_bool;

    /**
     * Construct the BooleanLiteralAst with the arguments matching the members.
     * @param[in] pos The position of the AST in the source code.
     * @param[in] tok_bool The token that represents the boolean literal.
     */
    explicit BooleanLiteralAst(
        decltype(tok_bool) &&tok_bool);
};


#endif //BOOLEAN_LITERAL_AST_HPP
