#ifndef ARRAY_LITERAL_0_ELEMENTS_HPP
#define ARRAY_LITERAL_0_ELEMENTS_HPP

#include <spp/asts/array_literal_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The ArrayLiteral0Elements AST represents an array literal with no elements, but with a specified element type and
 * size. This is used to create fixed-length arrays where the length is known at compile time, even if no elements are
 * provided.
 *
 * @n
 * All accesses into this literal will return the @c None variant of the @code Opt[T]@endcode type, @c T being the
 * element type, until values are set into the slots of the array. As there are no elements when created, and full type
 * information is required on declaration, the element type and size must be specified.
 */
struct spp::asts::ArrayLiteral0Elements final : ArrayLiteralAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The token that represents the left square bracket @code [@endcode in the array literal. This introduces the array
     * literal.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The type of the elements in the array. Every element in the array will be this type. Must be specified because
     * there are no elements to infer from.
     */
    std::unique_ptr<TypeAst> elem_type;

    /**
     * Separator token that represents the comma @code ,@endcode in the array literal. Separates the element type from
     * the number of elements this array will hold.
     */
    std::unique_ptr<TokenAst> tok_comma;

    /**
     * The number of elements in the array. The mapped type is @code std::Arr[T, n]@endcode, a fixed length array, so
     * the length must be known at compile time.
     */
    std::unique_ptr<TokenAst> size;

    /**
     * The token that represents the right square bracket @code ]@endcode in the array literal. This closes the array
     * literal.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the ArrayLiteral0Elements with the arguments matching the members.
     * @param[in] tok_l The token that represents the left square bracket \c [ in the array literal.
     * @param[in] elem_type The type of the elements in the array.
     * @param[in] tok_comma Separator token that represents the comma \c , in the array literal.
     * @param[in] size The number of elements in the array.
     * @param[in] tok_r The token that represents the right square bracket \c ] in the array literal.
     */
    ArrayLiteral0Elements(
        decltype(tok_l) &&tok_l,
        decltype(elem_type) &&elem_type,
        decltype(tok_comma) &&tok_comma,
        decltype(size) &&size,
        decltype(tok_r) &&tok_r);
};


#endif //ARRAY_LITERAL_0_ELEMENTS_HPP
