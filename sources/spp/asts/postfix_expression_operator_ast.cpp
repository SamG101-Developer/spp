module;
#include <spp/macros.hpp>

module spp.asts.postfix_expression_operator_ast;
import spp.asts.expression_ast;
import spp.asts.utils.ast_utils;

use_alias(spp::analyse::scopes, ExprSubst);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorAst::PostfixExpressionOperatorAst() = default;

spp::asts::PostfixExpressionOperatorAst::~PostfixExpressionOperatorAst() = default;

auto spp::asts::PostfixExpressionOperatorAst::RebuildsOperand() const -> bool {
  // Most operators work on their analysed operand.
  return false;
}

auto spp::asts::PostfixExpressionOperatorAst::WalksOperand() const -> bool {
  // The expression walks the operand, unless it was
  // never analysed in place.
  return RebuildsOperand();
}

auto spp::asts::PostfixExpressionOperatorAst::ExprParts() const -> Vec<IdentifierAst*> {
  return {};
}

auto spp::asts::PostfixExpressionOperatorAst::ReadExpr(
  ExprSubst const &) const -> Unique<PostfixExpressionOperatorAst> {
  // Default implementation that the non-specialized
  // postfix expression operators will use.
  return AstClone(this);
}

SPP_MOD_END
