module;
#include <spp/macros.hpp>

export module spp.asts.expression_ast;
import spp.asts.statement_ast;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct ArrayLiteralExplicitElementsAst;
  SPP_EXP_CLS struct ArrayLiteralRepeatedElementAst;
  SPP_EXP_CLS struct BooleanLiteralAst;
  SPP_EXP_CLS struct CharLiteralAst;
  SPP_EXP_CLS struct ExpressionAst;
  SPP_EXP_CLS struct FloatLiteralAst;
  SPP_EXP_CLS struct GenericArgumentAst;
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct IntegerLiteralAst;
  SPP_EXP_CLS struct StringLiteralAst;
  SPP_EXP_CLS struct TupleLiteralAst;
  SPP_EXP_CLS struct TypeAst; // TODO: GCC BUG REQUIRES THIS
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
  ExpressionAst();
  ~ExpressionAst() override;

  auto operator<=>(const ExpressionAst &) const -> Ordering;
  auto operator==(const ExpressionAst &) const -> bool;

  SPP_ATTR_NODISCARD virtual auto EqualsArrayLiteralExplicitElements(
    ArrayLiteralExplicitElementsAst const &) const -> Ordering;
  SPP_ATTR_NODISCARD virtual auto EqualsArrayLiteralRepeatedElement(
    ArrayLiteralRepeatedElementAst const &) const -> Ordering;
  SPP_ATTR_NODISCARD virtual auto EqualsBooleanLiteral(BooleanLiteralAst const &) const -> Ordering;
  SPP_ATTR_NODISCARD virtual auto EqualsCharLiteral(CharLiteralAst const &) const -> Ordering;
  SPP_ATTR_NODISCARD virtual auto EqualsFloatLiteral(FloatLiteralAst const &) const -> Ordering;
  SPP_ATTR_NODISCARD virtual auto EqualsIdentifier(IdentifierAst const &) const -> Ordering;
  SPP_ATTR_NODISCARD virtual auto EqualsIntegerLiteral(IntegerLiteralAst const &) const -> Ordering;
  SPP_ATTR_NODISCARD virtual auto EqualsStringLiteral(StringLiteralAst const &) const -> Ordering;
  SPP_ATTR_NODISCARD virtual auto EqualsTupleLiteral(TupleLiteralAst const &) const -> Ordering;
  SPP_ATTR_NODISCARD virtual auto EqualsTypeIdentifier(TypeIdentifierAst const &) const -> Ordering;
  SPP_ATTR_NODISCARD virtual auto EqualsTypeUnaryExpression(TypeUnaryExpressionAst const &) const -> Ordering;
  SPP_ATTR_NODISCARD virtual auto EqualsTypePostfixExpression(TypePostfixExpressionAst const &) const -> Ordering;
  SPP_ATTR_NODISCARD virtual auto Equals(ExpressionAst const &other) const -> Ordering;
  // Not "= 0" on purpose (postfix, unary etc)

  SPP_ATTR_NODISCARD virtual auto ExprParts() const -> Vec<Ast*>;

  /**
   * Rewrite every generic name written inside this expression against a set of arguments, answering with a new tree
   * rather than writing into this one. The expression counterpart of @c TypeAst::SubstituteGenerics , and the same job
   * one level up: the nodes that do real work are the ones holding a type, and they delegate to the type version.
   *
   * Needed wherever an expression is carried out of the scope that wrote it - a generic parameter's default, a function
   * parameter's default - because the names inside one are then read somewhere that has never heard of them. A default
   * of @c {Self::mo_seq_cst} materialised at a call site is read where @c Self names nothing at all.
   * @param args The bindings to rewrite against. An empty set answers with a plain clone.
   * @return A clone with the substitutions applied. Shared rather than unique, because a reader downstream takes a
   * @c shared_from_this of whatever it is given.
   */
  SPP_ATTR_NODISCARD virtual auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Shared<ExpressionAst>;
};
