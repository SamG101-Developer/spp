module;
#include <spp/macros.hpp>

module spp.asts.unary_expression_operator_ast;
import spp.asts.expression_ast;

use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::UnaryExpressionOperatorAst::UnaryExpressionOperatorAst() = default;
spp::asts::UnaryExpressionOperatorAst::~UnaryExpressionOperatorAst() = default;

auto spp::asts::UnaryExpressionOperatorAst::RebuildsOperand() const -> bool {
  // Most operators work on their analysed operand.
  return false;
}

auto spp::asts::UnaryExpressionOperatorAst::WalksOperand() const -> bool {
  // The expression walks the operand, unless it was never
  // analysed in place.
  return RebuildsOperand();
}
SPP_MOD_END
