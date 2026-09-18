module;
#include <spp/macros.hpp>

export module spp.asts.literal_ast;
import spp.asts.primary_expression_ast;
import std;

SPP_AST_COMMON_FWD_DECL(LiteralAst);

GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct ArrayLiteralRepeatedElementAst);
use(spp::asts, struct ArrayLiteralExplicitElementsAst);
use(spp::asts, struct BooleanLiteralAst);
use(spp::asts, struct CharLiteralAst);
use(spp::asts, struct FloatLiteralAst);
use(spp::asts, struct IntegerLiteralAst);
use(spp::asts, struct StringLiteralAst);
use(spp::asts, struct TupleLiteralAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::LiteralAst : PrimaryExpressionAst {
  LiteralAst();
  ~LiteralAst() override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;
};
