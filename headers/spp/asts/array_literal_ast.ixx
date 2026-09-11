module;
#include <spp/macros.hpp>

export module spp.asts.array_literal_ast;
import spp.asts.literal_ast;
import std;

SPP_AST_COMMON_FWD_DECL(ArrayLiteralAst) {
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct ArrayLiteralExplicitElementsAst;
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct ArrayLiteralRepeatedElementAst;
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct GenericArgumentAst;
  GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT SPP_EXP_CLS struct TypeAst;
}

/**
 * The ArrayLiteralAst is the base class for the two array literals: @c ArrayLiteral0Elements and
 * @c ArrayLiteralNElements. The common base class is for type checking only.
 */
SPP_EXP_CLS struct spp::asts::ArrayLiteralAst : LiteralAst {
  ArrayLiteralAst();

  ~ArrayLiteralAst() override;
};
