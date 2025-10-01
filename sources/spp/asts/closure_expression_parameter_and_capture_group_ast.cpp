#include <spp/pch.hpp>
#include <spp/analyse/scopes/scope.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/closure_expression_capture_ast.hpp>
#include <spp/asts/closure_expression_capture_group_ast.hpp>
#include <spp/asts/closure_expression_parameter_and_capture_group_ast.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/function_call_argument_ast.hpp>
#include <spp/asts/function_call_argument_group_ast.hpp>
#include <spp/asts/function_parameter_ast.hpp>
#include <spp/asts/function_parameter_group_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>

#include <genex/views/cast.hpp>
#include <genex/views/move.hpp>
#include <genex/views/to.hpp>


spp::asts::ClosureExpressionParameterAndCaptureGroupAst::ClosureExpressionParameterAndCaptureGroupAst(
    decltype(tok_l) &&tok_l,
    decltype(param_group) &&param_group,
    decltype(capture_group) &&capture_group,
    decltype(tok_r) &&tok_r) :
    Ast(),
    tok_l(std::move(tok_l)),
    param_group(std::move(param_group)),
    capture_group(std::move(capture_group)),
    tok_r(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->capture_group)
}


spp::asts::ClosureExpressionParameterAndCaptureGroupAst::~ClosureExpressionParameterAndCaptureGroupAst() = default;


auto spp::asts::ClosureExpressionParameterAndCaptureGroupAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::ClosureExpressionParameterAndCaptureGroupAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::ClosureExpressionParameterAndCaptureGroupAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<ClosureExpressionParameterAndCaptureGroupAst>(
        ast_clone(tok_l),
        ast_clone(param_group),
        ast_clone(capture_group),
        ast_clone(tok_r));
}


spp::asts::ClosureExpressionParameterAndCaptureGroupAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_APPEND(param_group);
    SPP_STRING_APPEND(capture_group);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::ClosureExpressionParameterAndCaptureGroupAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_APPEND(param_group);
    SPP_PRINT_APPEND(capture_group);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}


auto spp::asts::ClosureExpressionParameterAndCaptureGroupAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse the arguments against the outer scope's symbols (temp move asts).
    auto caps = capture_group->captures
        | genex::views::move
        | genex::views::cast_smart_ptr<FunctionCallArgumentAst>()
        | genex::views::to<std::vector>();
    const auto cap_group = std::make_unique<FunctionCallArgumentGroupAst>(nullptr, std::move(caps), nullptr);
    cap_group->stage_7_analyse_semantics(sm, meta);

    // New scope for parameters.
    auto scope_name = analyse::scopes::ScopeBlockName("<lambda-outer#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    capture_group->captures = cap_group->args
        | genex::views::move
        | genex::views::cast_smart_ptr<ClosureExpressionCaptureAst>()
        | genex::views::to<std::vector>();

    // Analyse the parameters and captures.
    param_group->stage_7_analyse_semantics(sm, meta);
    capture_group->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::ClosureExpressionParameterAndCaptureGroupAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse the arguments against the outer scope's symbols (temp move asts).
    auto caps = capture_group->captures
        | genex::views::move
        | genex::views::cast_smart_ptr<FunctionCallArgumentAst>()
        | genex::views::to<std::vector>();
    const auto cap_group = std::make_unique<FunctionCallArgumentGroupAst>(nullptr, std::move(caps), nullptr);
    cap_group->stage_8_check_memory(sm, meta);

    // New scope for parameters.
    sm->move_to_next_scope();
    capture_group->captures = cap_group->args
        | genex::views::move
        | genex::views::cast_smart_ptr<ClosureExpressionCaptureAst>()
        | genex::views::to<std::vector>();

    // Check the parameters and captures.
    param_group->stage_8_check_memory(sm, meta);
    capture_group->stage_8_check_memory(sm, meta);
}
