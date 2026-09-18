module;
#include <spp/macros.hpp>

export module spp.asts.unary_expression_operator_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;
import std;

SPP_AST_COMMON_FWD_DECL(UnaryExpressionOperatorAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::UnaryExpressionOperatorAst : Ast, mixins::TypeInferrableAst {
  UnaryExpressionOperatorAst();

  ~UnaryExpressionOperatorAst() override;
};
