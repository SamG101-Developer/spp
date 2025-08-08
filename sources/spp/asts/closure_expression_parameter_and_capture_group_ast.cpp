#include <algorithm>

#include <spp/asts/closure_expression_parameter_and_capture_group_ast.hpp>
#include <spp/asts/closure_expression_capture_group_ast.hpp>
#include <spp/asts/function_parameter_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::ClosureExpressionParameterAndCaptureGroupAst::ClosureExpressionParameterAndCaptureGroupAst(
    decltype(tok_l) &&tok_l,
    decltype(params) &&params,
    decltype(capture_group) &&capture_group,
    decltype(tok_r) &&tok_r):
    Ast(),
    tok_l(std::move(tok_l)),
    params(std::move(params)),
    capture_group(std::move(capture_group)),
    tok_r(std::move(tok_r)) {
}


spp::asts::ClosureExpressionParameterAndCaptureGroupAst::~ClosureExpressionParameterAndCaptureGroupAst() = default;


auto spp::asts::ClosureExpressionParameterAndCaptureGroupAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::ClosureExpressionParameterAndCaptureGroupAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


spp::asts::ClosureExpressionParameterAndCaptureGroupAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(params);
    SPP_STRING_APPEND(capture_group);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::ClosureExpressionParameterAndCaptureGroupAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(params);
    SPP_PRINT_APPEND(capture_group);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}
