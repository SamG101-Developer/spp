#ifndef CLOSURE_EXPRESSION_CAPTURE__AST_HPP
#define CLOSURE_EXPRESSION_CAPTURE__AST_HPP

#include <spp/asts/function_call_argument_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::ClosureExpressionCaptureAst final : FunctionCallArgumentAst {
    SPP_AST_KEY_FUNCTIONS;
    using FunctionCallArgumentAst::FunctionCallArgumentAst;

    ~ClosureExpressionCaptureAst() override;
};


#endif //CLOSURE_EXPRESSION_CAPTURE__AST_HPP
