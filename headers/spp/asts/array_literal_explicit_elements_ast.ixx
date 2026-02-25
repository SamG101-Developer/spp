module;
#include <spp/macros.hpp>

export module spp.asts.array_literal_explicit_elements_ast;
import spp.asts.array_literal_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ArrayLiteralExplicitElementsAst;
    SPP_EXP_CLS struct TokenAst;
}


/**
 * The ArrayLiteralExplicitElementsAst represents an array literal with a variable number of elements. This is used to create
 * an @code std::Arr[T, n]@endcode type, with @c T being the inferred type of each element (must all be the same type,
 * and @c n being the number of elements provided.
 *
 * @n
 * The elements are stored as @c Ast* pointers, but are restricted to expression-like ASTs by the parser. Their
 * respective analysis functions will be called by inheritance/vtable logic.
 */
SPP_EXP_CLS struct spp::asts::ArrayLiteralExplicitElementsAst final : ArrayLiteralAst {
    std::vector<std::unique_ptr<ExpressionAst>> elems;

    explicit ArrayLiteralExplicitElementsAst(
        decltype(elems) &&elements);
    ~ArrayLiteralExplicitElementsAst() override;
    auto to_rust() const -> std::string override;
};
