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
     * @param[in] tok_bool The token that represents the boolean literal.
     */
    explicit BooleanLiteralAst(
        decltype(tok_bool) &&tok_bool);

    static auto True(std::size_t pos) -> std::unique_ptr<BooleanLiteralAst>;

    static auto False(std::size_t pos) -> std::unique_ptr<BooleanLiteralAst>;
};


#endif //BOOLEAN_LITERAL_AST_HPP
