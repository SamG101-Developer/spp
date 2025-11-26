module;
#include <spp/macros.hpp>

export module spp.asts.closure_expression_capture_ast;
import spp.asts.function_call_argument_positional_ast;

import std;


SPP_EXP struct spp::asts::ClosureExpressionCaptureAst final : FunctionCallArgumentPositionalAst {
    ClosureExpressionCaptureAst(
        decltype(conv) &&conv,
        decltype(val) &&val);

    ~ClosureExpressionCaptureAst() override;

    SPP_AST_KEY_FUNCTIONS;
};
