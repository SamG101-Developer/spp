module;
#include <spp/macros.hpp>

export module spp.asts.closure_expression_capture_ast;
import spp.asts.ast_kind;
import spp.asts.function_call_argument_positional_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(ClosureExpressionCaptureAst) {
}

SPP_EXP_CLS struct spp::asts::ClosureExpressionCaptureAst final : FunctionCallArgumentPositionalAst {
  SPP_AST_KEY_FUNCTIONS(ClosureExpressionCaptureAst);

  ClosureExpressionCaptureAst(
    decltype(Conv) &&conv,
    decltype(Val) &&val);

  ~ClosureExpressionCaptureAst() override;
};
