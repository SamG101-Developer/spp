module;
#include <spp/macros.hpp>

module spp.asts.unary_expression_operator_ast;
import spp.asts.expression_ast;

SPP_MOD_BEGIN
UnaryExpressionOperatorAst::UnaryExpressionOperatorAst() = default;
UnaryExpressionOperatorAst::~UnaryExpressionOperatorAst() = default;

auto UnaryExpressionOperatorAst::RebuildsOperand() const -> bool {
  // Most operators work on their analysed operand.
  return false;
}

auto UnaryExpressionOperatorAst::WalksOperand() const -> bool {
  // The expression walks the operand, unless it was never
  // analysed in place.
  return RebuildsOperand();
}
SPP_MOD_END
