#pragma once
#include <spp/asts/function_call_argument_positional_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::ClosureExpressionCaptureAst final : FunctionCallArgumentPositionalAst {
    SPP_AST_KEY_FUNCTIONS;

    ClosureExpressionCaptureAst(
        decltype(conv) &&conv,
        decltype(val) &&val);

    ~ClosureExpressionCaptureAst() override;
};
