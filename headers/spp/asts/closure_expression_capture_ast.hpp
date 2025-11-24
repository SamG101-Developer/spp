#pragma once
#include <spp/asts/function_call_argument_positional_ast.hpp>


struct spp::asts::ClosureExpressionCaptureAst final : FunctionCallArgumentPositionalAst {
    ClosureExpressionCaptureAst(
        decltype(conv) &&conv,
        decltype(val) &&val);

    ~ClosureExpressionCaptureAst() override;

    SPP_AST_KEY_FUNCTIONS;
};
