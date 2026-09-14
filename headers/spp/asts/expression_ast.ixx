module;
#include <spp/macros.hpp>

export module spp.asts.expression_ast;
import spp.asts.ast;
import spp.asts.statement_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(ExpressionAst);
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

  /// Rewrite every generic name written inside this expression
  /// against a set of arguments, answering with a new tree rather
  /// than writing into this one. The expression counterpart of
  /// "TypeAst::SubstituteGenerics", one level up: the nodes that
  /// do real work are the ones holding a type, and they delegate
  /// to the type version.
  ///
  /// Needed wherever an expression is carried out of the scope
  /// that wrote it - a generic parameter's default, a function
  /// parameter's default - because its names are then read
  /// somewhere that has never heard of them. A default of
  /// "{Self::mo_seq_cst}" materialised at a call site is read
  /// where "Self" names nothing at all.
  ///
  /// An empty set answers with a plain clone. The clone is
  /// shared rather than unique, because a reader downstream
  /// takes a "shared_from_this" of whatever it is given.
  SPP_ATTR_NODISCARD virtual auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Shared<ExpressionAst>;
};
