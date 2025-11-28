module;
#include <spp/macros.hpp>

export module spp.asts.primary_expression_ast;
import spp.asts.expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct PrimaryExpressionAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypeUnaryExpressionAst;
    SPP_EXP_CLS struct TypePostfixExpressionAst;
}


SPP_EXP_CLS struct spp::asts::PrimaryExpressionAst : ExpressionAst {
    using ExpressionAst::ExpressionAst;

    ~PrimaryExpressionAst() override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;

protected:
    SPP_ATTR_NODISCARD auto equals_array_literal_explicit_elements(ArrayLiteralExplicitElementsAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals_array_literal_repeated_elements(ArrayLiteralRepeatedElementAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals_boolean_literal(BooleanLiteralAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals_float_literal(FloatLiteralAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals_identifier(IdentifierAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals_integer_literal(IntegerLiteralAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals_string_literal(StringLiteralAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals_tuple_literal(TupleLiteralAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals_type_identifier(TypeIdentifierAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals_type_unary_expression(TypeUnaryExpressionAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals_type_postfix_expression(TypePostfixExpressionAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals(ExpressionAst const &other) const -> std::strong_ordering override;
};
