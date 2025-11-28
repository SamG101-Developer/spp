module;
#include <spp/macros.hpp>

export module spp.asts.type_tuple_shorthand_ast;
import spp.asts._fwd;
import spp.asts.ast;
import spp.asts.mixins.temp_type_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct TypeTupleShorthandAst;
}


SPP_EXP_CLS struct spp::asts::TypeTupleShorthandAst final : virtual Ast, mixins::TempTypeAst {
    /**
     * The left parenthesis token that represents the start of the tuple type.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The types of the elements in the tuple.
     */
    std::vector<std::shared_ptr<TypeAst>> element_types;

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

    SPP_AST_KEY_FUNCTIONS;

    auto convert() -> std::unique_ptr<TypeAst> override;
};
