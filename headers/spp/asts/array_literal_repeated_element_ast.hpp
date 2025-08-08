#ifndef ARRAY_LITERAL_0_ELEMENTS_HPP
#define ARRAY_LITERAL_0_ELEMENTS_HPP

#include <spp/asts/array_literal_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The ArrayLiteralRepeatedElementAst represents an array literal with repeated elements and a size. This is used to
 * create fixed-length arrays where the length is known at compile time, and the element's type superimposes `Copy`.
 *
 * @n
 * All accesses into this literal will return the @c None variant of the @code Opt[T]@endcode type, @c T being the
 * element type, until values are set into the slots of the array. As there are no elements when created, and full type
 * information is required on declaration, the element type and size must be specified.
 */
struct spp::asts::ArrayLiteralRepeatedElementAst final : ArrayLiteralAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The token that represents the left square bracket @code [@endcode in the array literal. This introduces the array
     * literal.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The repeated element for the array. Every element in the array will be this value, by copying and placing in
     * contiguous memory.
     */
    std::unique_ptr<ExpressionAst> elem;

    /**
     * Separator token that represents the comma @code ;@endcode in the array literal. Separates the element value from
     * the number of elements this array will hold.
     */
    std::unique_ptr<TokenAst> tok_semicolon;

    /**
     * The number of elements in the array. The mapped type is @code std::Arr[T, n]@endcode, a fixed length array, so
     * the length must be known at compile time.
     */
    std::unique_ptr<ExpressionAst> size;

    /**
     * The token that represents the right square bracket @code ]@endcode in the array literal. This closes the array
     * literal.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the ArrayLiteral0Elements with the arguments matching the members.
     * @param[in] tok_l The token that represents the left square bracket @code [@endcode in the array literal.
     * @param[in] elem The repeated element for the array.
     * @param[in] tok_semicolon Separator token that represents the comma @code ;@endcode in the array literal.
     * @param[in] size The number of elements in the array.
     * @param[in] tok_r The token that represents the right square bracket @code ]@endcode in the array literal.
     */
    ArrayLiteralRepeatedElementAst(
        decltype(tok_l) &&tok_l,
        decltype(elem) &&elem,
        decltype(tok_semicolon) &&tok_semicolon,
        decltype(size) &&size,
        decltype(tok_r) &&tok_r);
};


#endif //ARRAY_LITERAL_0_ELEMENTS_HPP
