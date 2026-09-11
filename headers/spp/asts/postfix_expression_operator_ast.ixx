module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(PostfixExpressionOperatorAst) {
  SPP_EXP_CLS struct GenericArgumentAst;
  SPP_EXP_CLS struct IdentifierAst;
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorAst : Ast, mixins::TypeInferrableAst {
  PostfixExpressionOperatorAst();

  ~PostfixExpressionOperatorAst() override;

  SPP_ATTR_NODISCARD virtual auto ExprParts() const -> Vec<IdentifierAst*>;

  SPP_ATTR_NODISCARD virtual auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Unique<PostfixExpressionOperatorAst>;
};
