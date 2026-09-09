module;
#include <spp/macros.hpp>

export module spp.asts.unary_expression_operator_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct TypeAst;
  SPP_EXP_CLS struct UnaryExpressionOperatorAst;
}

SPP_EXP_CLS struct spp::asts::UnaryExpressionOperatorAst : Ast, mixins::TypeInferrableAst {
  SPP_GCC_VTABLE_FIX

  UnaryExpressionOperatorAst();

  ~UnaryExpressionOperatorAst() override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::UnaryExpressionOperatorAst)
