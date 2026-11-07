module;
#include <spp/macros.hpp>

module spp.asts.expression_ast;
import spp.analyse.scopes.symbols;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;

use_alias(spp::analyse::scopes, ExprSubst);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::asts::meta, MetaGuard);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::ExpressionAst::ExpressionAst() = default;
spp::asts::ExpressionAst::~ExpressionAst() = default;

auto spp::asts::ExpressionAst::Diverges(
  ScopeManager *sm, CompilerMetaData *meta) -> bool {
  // A "case" in a branch is asked for its type as if it were in
  // expression position, where a missing "else" is an error.
  const auto _meta_guard = MetaGuard(meta);
  meta->IgnoreMissingElseBranchForInference = true;
  return InferTypeRef(sm, meta).IsNever;
}

auto spp::asts::ExpressionAst::operator<=>(
  const ExpressionAst &rhs_expr) const -> Ordering {
  return Equals(rhs_expr);
}

auto spp::asts::ExpressionAst::operator==(
  const ExpressionAst &rhs_expr) const -> bool {
  return Equals(rhs_expr) == Ordering::equal;
}

auto spp::asts::ExpressionAst::EqualsArrayLiteralExplicitElements(
  ArrayLiteralExplicitElementsAst const &) const -> Ordering {
  return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsArrayLiteralRepeatedElement(
  ArrayLiteralRepeatedElementAst const &) const -> Ordering {
  return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsBooleanLiteral(
  BooleanLiteralAst const &) const -> Ordering {
  return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsCharLiteral(
  CharLiteralAst const &) const -> Ordering {
  return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsFloatLiteral(
  FloatLiteralAst const &) const -> Ordering {
  return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsIdentifier(
  IdentifierAst const &) const -> Ordering {
  return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsIntegerLiteral(
  IntegerLiteralAst const &) const -> Ordering {
  return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsStringLiteral(
  StringLiteralAst const &) const -> Ordering {
  return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsTupleLiteral(
  TupleLiteralAst const &) const -> Ordering {
  return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsTypeIdentifier(
  TypeIdentifierAst const &) const -> Ordering {
  return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsTypeUnaryExpression(
  TypeUnaryExpressionAst const &) const -> Ordering {
  return Ordering::less;
}

auto spp::asts::ExpressionAst::EqualsTypePostfixExpression(
  TypePostfixExpressionAst const &) const -> Ordering {
  return Ordering::less;
}

auto spp::asts::ExpressionAst::Equals(
  ExpressionAst const &) const -> Ordering {
  return Ordering::less;
}

auto spp::asts::ExpressionAst::ExprParts() const -> Vec<IdentifierAst*> {
  // The default "parts" list is empty, and certain asts
  // add a part.
  return {};
}

auto spp::asts::ExpressionAst::ReadExpr(
  analyse::scopes::ExprSubst const &) const -> Shared<ExpressionAst> {
  // The default operation is to do nothing, because all
  // other asts will specialize.
  return AstCloneShared(this);
}

SPP_MOD_END
