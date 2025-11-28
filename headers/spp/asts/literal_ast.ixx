module;
#include <spp/macros.hpp>

export module spp.asts.literal_ast;
import spp.asts.primary_expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ArrayLiteralExplicitElementsAst;
    SPP_EXP_CLS struct ArrayLiteralRepeatedElementAst;
    SPP_EXP_CLS struct BooleanLiteralAst;
    SPP_EXP_CLS struct FloatLiteralAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct IntegerLiteralAst;
    SPP_EXP_CLS struct LiteralAst;
    SPP_EXP_CLS struct StringLiteralAst;
    SPP_EXP_CLS struct TupleLiteralAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::LiteralAst : PrimaryExpressionAst {
    using PrimaryExpressionAst::PrimaryExpressionAst;

    ~LiteralAst() override;

// protected:
//     SPP_ATTR_NODISCARD auto equals_array_literal_explicit_elements(ArrayLiteralExplicitElementsAst const &) const -> std::strong_ordering override;
//     SPP_ATTR_NODISCARD auto equals_array_literal_repeated_elements(ArrayLiteralRepeatedElementAst const &) const -> std::strong_ordering override;
//     SPP_ATTR_NODISCARD auto equals_boolean_literal(BooleanLiteralAst const &) const -> std::strong_ordering override;
//     SPP_ATTR_NODISCARD auto equals_float_literal(FloatLiteralAst const &) const -> std::strong_ordering override;
//     SPP_ATTR_NODISCARD auto equals_integer_literal(IntegerLiteralAst const &) const -> std::strong_ordering override;
//     SPP_ATTR_NODISCARD auto equals_string_literal(StringLiteralAst const &) const -> std::strong_ordering override;
//     SPP_ATTR_NODISCARD auto equals_tuple_literal(TupleLiteralAst const &) const -> std::strong_ordering override;
//     SPP_ATTR_NODISCARD auto equals(ExpressionAst const &other) const -> std::strong_ordering override;
};
