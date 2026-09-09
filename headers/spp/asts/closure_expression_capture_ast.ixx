module;
#include <spp/macros.hpp>

export module spp.asts.closure_expression_capture_ast;
import spp.asts.ast_kind;
import spp.asts.function_call_argument_positional_ast;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct ClosureExpressionCaptureAst;
}

SPP_EXP_CLS struct spp::asts::ClosureExpressionCaptureAst final : FunctionCallArgumentPositionalAst {
  SPP_GCC_VTABLE_FIX
  SPP_AST_KEY_FUNCTIONS(ClosureExpressionCaptureAst);

  ClosureExpressionCaptureAst(
    decltype(Conv) &&conv,
    decltype(Val) &&val);

  ~ClosureExpressionCaptureAst() override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::ClosureExpressionCaptureAst)
