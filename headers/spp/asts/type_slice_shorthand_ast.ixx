module;
#include <spp/macros.hpp>

export module spp.asts.type_slice_shorthand_ast;
import spp.asts.ast;
import spp.asts.mixins.temp_type_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeSliceShorthandAst;
}

SPP_EXP_CLS struct spp::asts::TypeSliceShorthandAst final : Ast, mixins::TempTypeAst {
    /**
     * The left square bracket token that represents the start of the slice type.
     */
    Unique<TokenAst> TokL;

    /**
     * The type of the elements in the slice.
     */
    Shared<TypeAst> ElemType;

    /**
     * The right square bracket token that represents the end of the array type.
     */
    Unique<TokenAst> TokR;

    /**
     * Construct TypeSliceAst with the arguments matching the members.
     * @param tok_l The left square bracket token.
     * @param elem_type The type of the elements in the slice.
     * @param tok_r The right square bracket token.
     */
    TypeSliceShorthandAst(
        decltype(TokL) &&tok_l,
        decltype(ElemType) &&elem_type,
        decltype(TokR) &&tok_r);

    ~TypeSliceShorthandAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Convert() -> Unique<TypeAst> override;
};
