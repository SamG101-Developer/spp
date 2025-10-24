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


auto spp::asts::ClosureExpressionCaptureAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(conv);
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}
