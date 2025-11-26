module;
#include <spp/macros.hpp>

export module spp.asts.expression_ast;
import spp.asts.statement_ast;
import std;


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
SPP_EXP struct spp::asts::ExpressionAst : StatementAst {
    using StatementAst::StatementAst;
    friend struct ArrayLiteralExplicitElementsAst;
    friend struct ArrayLiteralRepeatedElementAst;
    friend struct BooleanLiteralAst;
    friend struct FloatLiteralAst;
    friend struct IdentifierAst;
    friend struct IntegerLiteralAst;
    friend struct StringLiteralAst;
    friend struct TupleLiteralAst;
    friend struct TypeIdentifierAst;
    friend struct TypeUnaryExpressionAst;
    friend struct TypePostfixExpressionAst;

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
    auto operator==(const ExpressionAst &) const -> bool;
};
