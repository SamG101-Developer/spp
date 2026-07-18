module;
#include <spp/macros.hpp>

export module spp.asts.type_array_shorthand_ast;
import spp.asts.ast;
import spp.asts.mixins.temp_type_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeArrayShorthandAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::TypeArrayShorthandAst final : Ast, mixins::TempTypeAst {
    /**
     * The left square bracket token that represents the start of the array type.
     */
    Unique<TokenAst> TokL;

    /**
     * The type of the elements in the array.
     */
    Shared<TypeAst> ElemType;

    /**
     * The @code ;@endcode token that separates the element type from the size in the array type declaration.
     */
    Unique<TokenAst> TokSemiColon;

    /**
     * The size of the array, which can be a literal or an expression.
     */
    Unique<ExpressionAst> Size;

    /**
     * The right square bracket token that represents the end of the array type.
     */
    Unique<TokenAst> TokR;

    /**
     * Construct the TypeArrayAst with the arguments matching the members.
     * @param tok_l The left square bracket token.
     * @param element_type The type of the elements in the array.
     * @param tok_semicolon The @code ;@endcode token that separates the element type from the size.
     * @param size The size of the array.
     * @param tok_r The right square bracket token.
     */
    TypeArrayShorthandAst(
        decltype(TokL) &&tok_l,
        decltype(ElemType) &&element_type,
        decltype(TokSemiColon) &&tok_semicolon,
        decltype(Size) &&size,
        decltype(TokR) &&tok_r);

    ~TypeArrayShorthandAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Convert() -> Unique<TypeAst> override;
};
