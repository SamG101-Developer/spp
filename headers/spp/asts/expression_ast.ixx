module;
#include <spp/macros.hpp>

export module spp.asts.expression_ast;
import spp.asts.statement_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ArrayLiteralExplicitElementsAst;
    SPP_EXP_CLS struct ArrayLiteralRepeatedElementAst;
    SPP_EXP_CLS struct BooleanLiteralAst;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct FloatLiteralAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct IntegerLiteralAst;
    SPP_EXP_CLS struct StringLiteralAst;
    SPP_EXP_CLS struct TupleLiteralAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypeUnaryExpressionAst;
    SPP_EXP_CLS struct TypePostfixExpressionAst;
}


/**
 * The ExpressionAst class is the base class for all expressions in the abstract syntax tree. It inherits from
 * StatementAst, allowing it to be used in places where a statement is expected, but also providing additional
 * functionality specific to expressions.
 *
 * @n
 * Other base classes inherit this class, such as the PrimaryExpressionAst, which represents the most basic form of
 * expressions. Other expression such as unary, postfix, and binary expressions also inherit this class.
 *
 * @note This class is more of a "marker" to see where expressions should be, as there is no additional functionality in
 * this class.
 */
SPP_EXP_CLS struct spp::asts::ExpressionAst : StatementAst {
    using StatementAst::StatementAst;
    friend struct spp::asts::ArrayLiteralExplicitElementsAst;
    friend struct spp::asts::ArrayLiteralRepeatedElementAst;
    friend struct spp::asts::BooleanLiteralAst;
    friend struct spp::asts::FloatLiteralAst;
    friend struct spp::asts::IdentifierAst;
    friend struct spp::asts::IntegerLiteralAst;
    friend struct spp::asts::StringLiteralAst;
    friend struct spp::asts::TupleLiteralAst;
    friend struct spp::asts::TypeIdentifierAst;
    friend struct spp::asts::TypeUnaryExpressionAst;
    friend struct spp::asts::TypePostfixExpressionAst;

protected:
    SPP_ATTR_NODISCARD virtual auto equals_array_literal_explicit_elements(ArrayLiteralExplicitElementsAst const &) const -> std::strong_ordering;
    SPP_ATTR_NODISCARD virtual auto equals_array_literal_repeated_elements(ArrayLiteralRepeatedElementAst const &) const -> std::strong_ordering;
    SPP_ATTR_NODISCARD virtual auto equals_boolean_literal(BooleanLiteralAst const &) const -> std::strong_ordering;
    SPP_ATTR_NODISCARD virtual auto equals_float_literal(FloatLiteralAst const &) const -> std::strong_ordering;
    SPP_ATTR_NODISCARD virtual auto equals_identifier(IdentifierAst const &) const -> std::strong_ordering;
    SPP_ATTR_NODISCARD virtual auto equals_integer_literal(IntegerLiteralAst const &) const -> std::strong_ordering;
    SPP_ATTR_NODISCARD virtual auto equals_string_literal(StringLiteralAst const &) const -> std::strong_ordering;
    SPP_ATTR_NODISCARD virtual auto equals_tuple_literal(TupleLiteralAst const &) const -> std::strong_ordering;
    SPP_ATTR_NODISCARD virtual auto equals_type_identifier(TypeIdentifierAst const &) const -> std::strong_ordering;
    SPP_ATTR_NODISCARD virtual auto equals_type_unary_expression(TypeUnaryExpressionAst const &) const -> std::strong_ordering;
    SPP_ATTR_NODISCARD virtual auto equals_type_postfix_expression(TypePostfixExpressionAst const &) const -> std::strong_ordering;
    SPP_ATTR_NODISCARD virtual auto equals(ExpressionAst const &other) const -> std::strong_ordering;

public:
    ExpressionAst(ExpressionAst const &other);
    ~ExpressionAst() override;
    auto operator<=>(const ExpressionAst &) const -> std::strong_ordering;
    virtual auto operator==(const ExpressionAst &) const -> bool;
};
