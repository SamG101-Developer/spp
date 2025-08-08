#include <algorithm>

#include <spp/asts/closure_expression_capture_ast.hpp>
#include <spp/asts/closure_expression_capture_group_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::ClosureExpressionCaptureGroupAst::ClosureExpressionCaptureGroupAst(
    decltype(tok_caps) &&tok_caps,
    decltype(captures) &&captures):
    Ast(),
    tok_caps(std::move(tok_caps)),
    captures(std::move(captures)) {
}


spp::asts::ClosureExpressionCaptureGroupAst::~ClosureExpressionCaptureGroupAst() = default;


auto spp::asts::ClosureExpressionCaptureGroupAst::pos_start() const -> std::size_t {
    return tok_caps->pos_start();
}


auto spp::asts::ClosureExpressionCaptureGroupAst::pos_end() const -> std::size_t {
    return captures.back()->pos_end();
}


spp::asts::ClosureExpressionCaptureGroupAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_caps);
    SPP_STRING_EXTEND(captures);
    SPP_STRING_END;
}


auto spp::asts::ClosureExpressionCaptureGroupAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_caps);
    SPP_PRINT_EXTEND(captures);
    SPP_PRINT_END;
}
