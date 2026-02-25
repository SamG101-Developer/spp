module;
#include <spp/macros.hpp>

export module spp.asts.array_literal_repeated_element_ast;
import spp.asts.array_literal_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ArrayLiteralRepeatedElementAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The ArrayLiteralRepeatedElementAst represents an array literal with repeated elements and a size. This is used to
 * create fixed-length arrays where the length is known at compile time, and the element's type superimposes `Copy`.
 *
 * @n
 * All accesses into this literal will return the @c None variant of the @code Opt[T]@endcode type, @c T being the
 * element type, until values are set into the slots of the array. As there are no elements when created, and full type
 * information is required on declaration, the element type and size must be specified.
 */
SPP_EXP_CLS struct spp::asts::ArrayLiteralRepeatedElementAst final : ArrayLiteralAst {
    std::unique_ptr<ExpressionAst> elem;
    std::unique_ptr<ExpressionAst> size;

    explicit ArrayLiteralRepeatedElementAst(
        decltype(elem) &&elem,
        decltype(size) &&size);
    ~ArrayLiteralRepeatedElementAst() override;
    auto to_rust() const -> std::string override;
};
