module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(PostfixExpressionOperatorAst);
use(spp::analyse::scopes, struct ExprSubst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct IdentifierAst);

GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorAst : Ast, mixins::TypeInferrableAst {
  /// The operand as written, taken before it is analysed; only
  /// set for an operator that rebuilds its operand
  /// ("RebuildsOperand").
  Unique<ExpressionAst> WrittenOperand;

  /// The left-hand side the operator applies to: its expression's
  /// "Lhs", not owned, set by the expression before each stage
  /// and query it runs the operator for.
  ExpressionAst *Lhs = nullptr;

  PostfixExpressionOperatorAst();

  ~PostfixExpressionOperatorAst() override;

  /// Whether the operator builds its own form from its operand as
  /// written ("?" into a "case"), which is what the later
  /// stages walk. The expression does not analyse the operand
  /// then: it hands the written copy over in "WrittenOperand",
  /// and the operator owns every analysis of it.
  SPP_ATTR_NODISCARD virtual auto RebuildsOperand() const -> bool;

  /// Whether the operator walks its operand's later stages itself
  /// (through the call it maps to, or its own form), rather than
  /// the expression walking it first. An operator that rebuilds its
  /// operand always does.
  SPP_ATTR_NODISCARD virtual auto WalksOperand() const -> bool;

  SPP_ATTR_NODISCARD virtual auto ExprParts() const -> Vec<IdentifierAst*>;

  SPP_ATTR_NODISCARD virtual auto ReadExpr(
    ExprSubst const &sub) const -> Unique<PostfixExpressionOperatorAst>;
};
