module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(PostfixExpressionOperatorAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct IdentifierAst);

GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorAst : Ast, mixins::TypeInferrableAst {
  PostfixExpressionOperatorAst();

  ~PostfixExpressionOperatorAst() override;

  SPP_ATTR_NODISCARD virtual auto ExprParts() const -> Vec<IdentifierAst*>;

  SPP_ATTR_NODISCARD virtual auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Unique<PostfixExpressionOperatorAst>;
};
