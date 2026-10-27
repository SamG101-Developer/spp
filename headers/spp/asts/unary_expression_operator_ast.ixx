module;
#include <spp/macros.hpp>

export module spp.asts.unary_expression_operator_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(UnaryExpressionOperatorAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::UnaryExpressionOperatorAst : Ast, mixins::TypeInferrableAst {
  /// The operand as written, taken before it is analysed; only
  /// set for an operator that rebuilds its operand
  /// ("RebuildsOperand").
  Unique<ExpressionAst> WrittenOperand;

  UnaryExpressionOperatorAst();

  ~UnaryExpressionOperatorAst() override;

  /// Whether the operator builds its own form from its operand as
  /// written ("async" into a closure), which is what the later
  /// stages walk. The expression does not analyse the operand
  /// then: it hands the written copy over in "WrittenOperand",
  /// and the operator owns every analysis of it.
  SPP_ATTR_NODISCARD virtual auto RebuildsOperand() const -> bool;

  /// Whether the operator walks its operand's later stages itself
  /// (through the call it maps to, or its own form), rather than
  /// the expression walking it first. An operator that rebuilds its
  /// operand always does.
  SPP_ATTR_NODISCARD virtual auto WalksOperand() const -> bool;
};
