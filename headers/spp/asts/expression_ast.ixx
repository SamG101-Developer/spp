module;
#include <spp/macros.hpp>

export module spp.asts.expression_ast;
import spp.asts.ast;
import spp.asts.statement_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(ExpressionAst);
use(spp::analyse::scopes, struct ExprSubst);
use(spp::asts, struct ArrayLiteralExplicitElementsAst);
use(spp::asts, struct ArrayLiteralRepeatedElementAst);
use(spp::asts, struct BooleanLiteralAst);
use(spp::asts, struct CharLiteralAst);
use(spp::asts, struct FloatLiteralAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct IntegerLiteralAst);
use(spp::asts, struct StringLiteralAst);
use(spp::asts, struct TupleLiteralAst);
use(spp::asts, struct TypeIdentifierAst);
use(spp::asts, struct TypeUnaryExpressionAst);
use(spp::asts, struct TypePostfixExpressionAst);
use(spp::analyse::scopes, struct TypeRef);

GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT
use(spp::asts, struct TypeAst);

/// The base class for all expressions. It inherits StatementAst,
/// so it can be used where a statement is expected, while adding
/// functionality specific to expressions.
///
/// Other base classes inherit this, such as PrimaryExpressionAst
/// for the most basic expressions; unary, postfix and binary
/// expressions inherit it too. It is mostly a "marker" for where
/// expressions should be, with no additional functionality.
SPP_EXP_CLS struct spp::asts::ExpressionAst : StatementAst {
  ExpressionAst();
  ~ExpressionAst() override;

  auto operator<=>(const ExpressionAst &) const -> Ordering;
  auto operator==(const ExpressionAst &) const -> bool;

  SPP_ATTR_NODISCARD virtual auto EqualsArrayLiteralExplicitElements(
    ArrayLiteralExplicitElementsAst const &) const
    -> Ordering;
  SPP_ATTR_NODISCARD virtual auto EqualsArrayLiteralRepeatedElement(
    ArrayLiteralRepeatedElementAst const &) const
    -> Ordering;
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

  SPP_ATTR_NODISCARD virtual auto ExprParts() const -> Vec<IdentifierAst*>;

  /// Look through expression asts for types in the asts, such
  /// as the object initializer ast containing a type. Lots of
  /// asts override this to look into its members.
  SPP_ATTR_NODISCARD virtual auto ReadExpr(
    analyse::scopes::ExprSubst const &sub) const
    -> Shared<ExpressionAst>;
};
