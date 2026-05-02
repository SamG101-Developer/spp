module;
#include <spp/macros.hpp>

export module spp.asts.type_tuple_shorthand_ast;
import spp.asts.ast;
import spp.asts.mixins.temp_type_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TypeTupleShorthandAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::TypeTupleShorthandAst final : Ast, mixins::TempTypeAst {
    /**
     * The left parenthesis token that represents the start of the tuple type.
     */
    Unique<TokenAst> TokL;

    /**
     * The types of the elements in the tuple.
     */
    Vec<Shared<TypeAst>> ElemTypes;

    /**
     * The right parenthesis token that represents the end of the tuple type.
     */
    Unique<TokenAst> TokR;

    /**
     * Construct the TypeTupleShorthandAst with the arguments matching the members.
     * @param tok_l The left parenthesis token.
     * @param element_types The types of the elements in the tuple.
     * @param tok_r The right parenthesis token.
     */
    TypeTupleShorthandAst(
        decltype(TokL) &&tok_l,
        decltype(ElemTypes) &&element_types,
        decltype(TokR) &&tok_r);

    ~TypeTupleShorthandAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Convert() -> Unique<TypeAst> override;
};
