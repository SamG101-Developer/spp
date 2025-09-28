#include <spp/asts/ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/mixins/compiler_stages.hpp>


auto spp::asts::mixins::CompilerStages::stage_1_pre_process(Ast *) -> void {
}


auto spp::asts::mixins::CompilerStages::stage_2_gen_top_level_scopes(ScopeManager *, CompilerMetaData *) -> void {
}


auto spp::asts::mixins::CompilerStages::stage_3_gen_top_level_aliases(ScopeManager *, CompilerMetaData *) -> void {
}


auto spp::asts::mixins::CompilerStages::stage_4_qualify_types(ScopeManager *, CompilerMetaData *) -> void {
}


auto spp::asts::mixins::CompilerStages::stage_5_load_super_scopes(ScopeManager *, CompilerMetaData *) -> void {
}


auto spp::asts::mixins::CompilerStages::stage_6_pre_analyse_semantics(ScopeManager *, CompilerMetaData *) -> void {
}


auto spp::asts::mixins::CompilerStages::stage_7_analyse_semantics(ScopeManager *, CompilerMetaData *) -> void {
}


auto spp::asts::mixins::CompilerStages::stage_8_check_memory(ScopeManager *, CompilerMetaData *) -> void {
}


auto spp::asts::mixins::CompilerStages::stage_9_code_gen_1() -> void {
}


auto spp::asts::mixins::CompilerStages::stage_10_code_gen_2() -> void {
}


auto spp::asts::mixins::CompilerMetaData::save() -> void {
    m_history.emplace(
        current_stage, return_type_overload_resolver_type, assignment_target,
        assignment_target_type, ignore_missing_else_branch_for_inference, case_condition, cls_sym,
        enclosing_function_scope, enclosing_function_flavour, enclosing_function_ret_type,
        current_lambda_outer_scope, target_call_function_prototype, target_call_was_function_async,
        prevent_auto_generator_resume, let_stmt_explicit_type, let_stmt_value, loop_double_check_active,
        current_loop_depth, current_loop_ast, loop_return_types, object_init_type, infer_source,
        infer_target, postfix_expression_lhs, unary_expression_rhs, skip_type_analysis_generic_checks,
        type_analysis_type_scope);
}


auto spp::asts::mixins::CompilerMetaData::restore() -> void {
    auto state = std::move(m_history.top()); // *DO NOT* click "convert to structured bindings (CLion) -- LAG"
    m_history.pop();
    current_stage = state.current_stage;
    return_type_overload_resolver_type = state.return_type_overload_resolver_type;
    assignment_target = state.assignment_target;
    assignment_target_type = state.assignment_target_type;
    ignore_missing_else_branch_for_inference = state.ignore_missing_else_branch_for_inference;
    case_condition = state.case_condition;
    cls_sym = state.cls_sym;
    enclosing_function_scope = state.enclosing_function_scope;
    enclosing_function_flavour = state.enclosing_function_flavour;
    enclosing_function_ret_type = state.enclosing_function_ret_type;
    current_lambda_outer_scope = state.current_lambda_outer_scope;
    target_call_function_prototype = state.target_call_function_prototype;
    target_call_was_function_async = state.target_call_was_function_async;
    prevent_auto_generator_resume = state.prevent_auto_generator_resume;
    let_stmt_explicit_type = state.let_stmt_explicit_type;
    let_stmt_value = state.let_stmt_value;
    loop_double_check_active = state.loop_double_check_active;
    current_loop_depth = state.current_loop_depth;
    current_loop_ast = state.current_loop_ast;
    loop_return_types = state.loop_return_types;
    object_init_type = state.object_init_type;
    infer_source = state.infer_source;
    infer_target = state.infer_target;
    postfix_expression_lhs = state.postfix_expression_lhs;
    unary_expression_rhs = state.unary_expression_rhs;
    skip_type_analysis_generic_checks = state.skip_type_analysis_generic_checks;
    type_analysis_type_scope = state.type_analysis_type_scope;
}
