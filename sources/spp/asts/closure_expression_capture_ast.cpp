module;
#include <spp/macros.hpp>

module spp.asts.closure_expression_capture_ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;


spp::asts::ClosureExpressionCaptureAst::ClosureExpressionCaptureAst(
    decltype(conv) &&conv,
    decltype(val) &&val) :
    FunctionCallArgumentPositionalAst(std::move(conv), nullptr, std::move(val)) {
}


spp::asts::ClosureExpressionCaptureAst::~ClosureExpressionCaptureAst() = default;


auto spp::asts::ClosureExpressionCaptureAst::pos_start() const
    -> std::size_t {
    return conv ? conv->pos_start() : val->pos_start();
}


auto spp::asts::ClosureExpressionCaptureAst::pos_end() const
    -> std::size_t {
    return val->pos_end();
}


auto spp::asts::ClosureExpressionCaptureAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<ClosureExpressionCaptureAst>(
        ast_clone(conv),
        ast_clone(val));
}


spp::asts::ClosureExpressionCaptureAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(conv);
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}
