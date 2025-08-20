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
    m_history.emplace(ast_clone(return_type_overload_resolver_type), assignment_targets);
}


auto spp::asts::mixins::CompilerMetaData::restore() -> void {
    auto state = std::move(m_history.top());
    m_history.pop();
    return_type_overload_resolver_type = std::move(state.return_type_overload_resolver_type);
    assignment_targets = std::move(state.assignment_targets);
    assignment_target_type = std::move(state.assignment_target_type);
    ignore_missing_else_branch_for_inference = state.ignore_missing_else_branch_for_inference;
    case_condition = state.case_condition;
    // todo: finish
}
