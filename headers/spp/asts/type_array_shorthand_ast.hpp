#pragma once
#include <spp/asts/ast.hpp>
#include <spp/asts/mixins/temp_type_ast.hpp>


struct spp::asts::TypeArrayShorthandAst final : virtual Ast, mixins::TempTypeAst {
    /**
     * The left square bracket token that represents the start of the array type.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The type of the elements in the array.
     */
    std::shared_ptr<TypeAst> element_type;

    /**
     * The @code ;@endcode token that separates the element type from the size in the array type declaration.
     */
    std::unique_ptr<TokenAst> tok_semicolon;

    /**
     * The size of the array, which can be a literal or an expression.
     */
    std::unique_ptr<ExpressionAst> size;

    /**
     * The right square bracket token that represents the end of the array type.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the TypeArrayAst with the arguments matching the members.
     * @param tok_l The left square bracket token.
     * @param element_type The type of the elements in the array.
     * @param tok_semicolon The @code ;@endcode token that separates the element type from the size.
     * @param size The size of the array.
     * @param tok_r The right square bracket token.
     */
    TypeArrayShorthandAst(
        decltype(tok_l) &&tok_l,
        decltype(element_type) &&element_type,
        decltype(tok_semicolon) &&tok_semicolon,
        decltype(size) &&size,
        decltype(tok_r) &&tok_r);

    ~TypeArrayShorthandAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto convert() -> std::unique_ptr<TypeAst> override;
};
