module;
#include <spp/macros.hpp>

export module spp.asts.primary_expression_ast;
import spp.asts.expression_ast;
import std;

SPP_AST_COMMON_FWD_DECL(PrimaryExpressionAst);

GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeIdentifierAst);

SPP_EXP_CLS struct spp::asts::PrimaryExpressionAst : ExpressionAst {
  PrimaryExpressionAst();
  ~PrimaryExpressionAst() override;
};
