#ifndef TUPLE_LITERAL_AST_HPP
#define TUPLE_LITERAL_AST_HPP

#include <spp/asts/literal_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::TupleLiteralAst final : LiteralAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The left parenthesis token that represents the start of the tuple literal.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The elements of the tuple literal. This is a list of expressions that are contained within the tuple.
     */
    std::vector<std::unique_ptr<ExpressionAst>> elements;

    /**
     * The right parenthesis token that represents the end of the tuple literal.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the TupleLiteralAst with the arguments matching the members.
     * @param tok_l The left parenthesis token.
     * @param elements The elements of the tuple literal.
     * @param tok_r The right parenthesis token.
     */
    TupleLiteralAst(
        decltype(tok_l) &&tok_l,
        decltype(elements) &&elements,
        decltype(tok_r) &&tok_r);

    ~TupleLiteralAst() override;
};


#endif //TUPLE_LITERAL_AST_HPP
