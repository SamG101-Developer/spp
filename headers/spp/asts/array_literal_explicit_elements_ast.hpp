#ifndef ARRAY_LITERAL_N_ELEMENTS_HPP
#define ARRAY_LITERAL_N_ELEMENTS_HPP

#include <spp/asts/array_literal_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The ArrayLiteralExplicitElementsAst represents an array literal with a variable number of elements. This is used to create
 * an @code std::Arr[T, n]@endcode type, with @c T being the inferred type of each element (must all be the same type,
 * and @c n being the number of elements provided.
 *
 * @n
 * The elements are stored as @c Ast* pointers, but are restricted to expression-like ASTs by the parser. Their
 * respective analysis functions will be called by inheritance/vtable logic.
 */
struct spp::asts::ArrayLiteralExplicitElements final : ArrayLiteralAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The token that represents the left square bracket \c [ in the array literal. This introduces the array literal.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The list of expressions that are the elements of the array. Each element is an AST that represents an expression.
     * They will all infer to the same type.
     */
    std::vector<std::unique_ptr<ExpressionAst>> elements;

    /**
     * The token that represents the right square bracket \c ] in the array literal. This closes the array literal.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the ArrayLiteralNElements with the arguments matching the members.
     * @param[in] pos The position of the AST in the source code.
     * @param[in] tok_l The token that represents the left square bracket \c [ in the array literal.
     * @param[in] elements The list of expressions that are the elements of the array.
     * @param[in] tok_r The token that represents the right square bracket \c ] in the array literal.
     */
    ArrayLiteralExplicitElements(
        decltype(tok_l) &&tok_l,
        decltype(elements) &&elements,
        decltype(tok_r) &&tok_r);
};


#endif //ARRAY_LITERAL_N_ELEMENTS_HPP
