module;
#include <spp/macros.hpp>

export module spp.asts.primary_expression_ast;
import spp.asts.expression_ast;
import std;

SPP_AST_COMMON_FWD_DECL(PrimaryExpressionAst) {
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct ExpressionAst;
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct GenericArgumentAst;
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct IdentifierAst;
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct TypeAst;
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct TypeIdentifierAst;
}

SPP_EXP_CLS struct spp::asts::PrimaryExpressionAst : ExpressionAst {
  PrimaryExpressionAst();

  ~PrimaryExpressionAst() override;
};
