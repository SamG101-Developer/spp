#include <spp/asts/closure_expression_capture_ast.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::ClosureExpressionCaptureAst::ClosureExpressionCaptureAst(
    decltype(conv) &&conv,
    decltype(val) &&val) :
    FunctionCallArgumentPositionalAst(std::move(conv), nullptr, std::move(val)) {
}


spp::asts::ClosureExpressionCaptureAst::~ClosureExpressionCaptureAst() = default;
