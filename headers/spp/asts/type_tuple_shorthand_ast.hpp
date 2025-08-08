#ifndef TYPE_TUPLE_SHORTHAND_AST_HPP
#define TYPE_TUPLE_SHORTHAND_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/mixins/temp_type_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::TypeTupleShorthandAst final : virtual Ast, mixins::TempTypeAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The left parenthesis token that represents the start of the tuple type.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The types of the elements in the tuple.
     */
    std::vector<std::unique_ptr<TypeAst>> element_types;

    /**
     * The right parenthesis token that represents the end of the tuple type.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the TypeTupleShorthandAst with the arguments matching the members.
     * @param tok_l The left parenthesis token.
     * @param element_types The types of the elements in the tuple.
     * @param tok_r The right parenthesis token.
     */
    TypeTupleShorthandAst(
        decltype(tok_l) &&tok_l,
        decltype(element_types) &&element_types,
        decltype(tok_r) &&tok_r);

    ~TypeTupleShorthandAst() override;

    auto convert() -> std::unique_ptr<TypeAst> override;
};


#endif //TYPE_TUPLE_SHORTHAND_AST_HPP
