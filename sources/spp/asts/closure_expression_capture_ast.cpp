#include <spp/asts/closure_expression_capture_ast.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/identifier_ast.hpp>


spp::asts::ClosureExpressionCaptureAst::ClosureExpressionCaptureAst(
    decltype(conv) &&conv,
    decltype(name) &&name):
    Ast(),
    conv(std::move(conv)),
    name(std::move(name)) {
}


spp::asts::ClosureExpressionCaptureAst::~ClosureExpressionCaptureAst() = default;


auto spp::asts::ClosureExpressionCaptureAst::pos_start() const -> std::size_t {
    return conv ? conv->pos_start() : name->pos_start();
}


auto spp::asts::ClosureExpressionCaptureAst::pos_end() const -> std::size_t {
    return name->pos_end();
}


spp::asts::ClosureExpressionCaptureAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(conv);
    SPP_STRING_APPEND(name);
    SPP_STRING_END;
}


auto spp::asts::ClosureExpressionCaptureAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(conv);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_END;
}
