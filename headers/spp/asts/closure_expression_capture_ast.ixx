module;
#include <spp/macros.hpp>

export module spp.asts.closure_expression_capture_ast;
import spp.asts.function_call_argument_positional_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct ClosureExpressionCaptureAst;
}


SPP_EXP_CLS struct spp::asts::ClosureExpressionCaptureAst final : FunctionCallArgumentPositionalAst {
    ClosureExpressionCaptureAst(
        decltype(conv) &&conv,
        decltype(val) &&val);

    ~ClosureExpressionCaptureAst() override;

    SPP_AST_KEY_FUNCTIONS;
};
