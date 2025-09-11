#pragma once
#include <spp/asts/array_literal_ast.hpp>


/**
 * The ArrayLiteralExplicitElementsAst represents an array literal with a variable number of elements. This is used to create
 * an @code std::Arr[T, n]@endcode type, with @c T being the inferred type of each element (must all be the same type,
 * and @c n being the number of elements provided.
 *
 * @n
 * The elements are stored as @c Ast* pointers, but are restricted to expression-like ASTs by the parser. Their
 * respective analysis functions will be called by inheritance/vtable logic.
 */
struct spp::asts::ArrayLiteralExplicitElementsAst final : ArrayLiteralAst {
    SPP_AST_KEY_FUNCTIONS;

protected:
    auto equals_array_literal_explicit_elements(ArrayLiteralExplicitElementsAst const &) const -> std::strong_ordering override;

    auto equals(ExpressionAst const &other) const -> std::strong_ordering override;

public:
    /**
     * The token that represents the left square bracket @code [@endcode in the array literal. This introduces the array
     * literal.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The list of expressions that are the elements of the array. Each element is an AST that represents an expression.
     * They will all infer to the same type.
     */
    std::vector<std::unique_ptr<ExpressionAst>> elems;

    /**
     * The token that represents the right square bracket @code ]@endcode in the array literal. This closes the array
     * literal.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the ArrayLiteralNElements with the arguments matching the members.
     * @param[in] tok_l The token that represents the left square bracket \c [ in the array literal.
     * @param[in] elements The list of expressions that are the elements of the array.
     * @param[in] tok_r The token that represents the right square bracket \c ] in the array literal.
     */
    ArrayLiteralExplicitElementsAst(
        decltype(tok_l) &&tok_l,
        decltype(elems) &&elements,
        decltype(tok_r) &&tok_r);

    ~ArrayLiteralExplicitElementsAst() override;

    /**
     * Semantic analysis for an array with explicit elements ensures that all elements are of the same type, and that
     * none of the elements are borrowed (ie the type of all the elements is not a borrow type). This is because it
     * would otherwise be a violation of the second class borrow memory safety model.
     * @param [in] sm The scope manager to find the symbols of the elements in.
     * @param [in,out] meta Associated metadata.
     * @throw spp::analyse::errors::SppTypeMismatchError if the elements are not of the same type.
     * @throw spp::analyse::errors::SppSecondClassBorrowViolationError if any of the elements are borrowed.
     */
    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    /**
     * Check the memory state of the element being repeated (mostly to ensure that it is initialised and not just a
     * valid type).
     * @param sm The scope manager to use for memory checking.
     * @param meta Associated metadata.
     */
    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    /**
     * The inferred type of an array literal is always @code std::array::Arr[T, n]@endcode, where @c T is the type of
     * the elements in the array literal, and @c n is the number of elements in the array literal.
     * @param [in] sm The scope manager to use for type inference.
     * @param [in,out] meta Associated metadata.
     * @return The @code std::array::Arr[T, n]@endcode type of the array literal.
     */
    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
