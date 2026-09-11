module;
#include <spp/macros.hpp>

export module spp.asts.literal_ast;
import spp.asts.primary_expression_ast;
import std;

SPP_AST_COMMON_FWD_DECL(LiteralAst) {
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct GenericArgumentAst;
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct ArrayLiteralRepeatedElementAst;
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct ArrayLiteralExplicitElementsAst;
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct BooleanLiteralAst;
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct CharLiteralAst;
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct FloatLiteralAst;
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct IntegerLiteralAst;
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct StringLiteralAst;
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct TupleLiteralAst;
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::LiteralAst : PrimaryExpressionAst {
  LiteralAst();

  ~LiteralAst() override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const
    -> bool override;
};
