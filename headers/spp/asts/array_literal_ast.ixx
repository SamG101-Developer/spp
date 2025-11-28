module;
#include <spp/macros.hpp>

export module spp.asts.array_literal_ast;
import spp.asts.literal_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ArrayLiteralAst;
    SPP_EXP_CLS struct ArrayLiteralExplicitElementsAst;
    SPP_EXP_CLS struct ArrayLiteralRepeatedElementAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The ArrayLiteralAst is the base class for the two array literals: @c ArrayLiteral0Elements and
 * @c ArrayLiteralNElements. The common base class is for type checking only.
 */
SPP_EXP_CLS struct spp::asts::ArrayLiteralAst : LiteralAst {
    using LiteralAst::LiteralAst;

    ~ArrayLiteralAst() override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;

protected:
    SPP_ATTR_NODISCARD auto equals_array_literal_explicit_elements(ArrayLiteralExplicitElementsAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals_array_literal_repeated_elements(ArrayLiteralRepeatedElementAst const &) const -> std::strong_ordering override;
    auto equals(ExpressionAst const &other) const -> std::strong_ordering override;
};
