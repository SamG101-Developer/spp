#include <spp/asts/mixins/compiler_stages.hpp>


auto spp::asts::mixins::CompilerStages::pre_process(Ast *) -> void {
}


auto spp::asts::mixins::CompilerStages::gen_top_level_scopes(ScopeManager *) -> void {
}


auto spp::asts::mixins::CompilerStages::gen_top_level_aliases(ScopeManager *, CompilerMetaData *) -> void {
}


auto spp::asts::mixins::CompilerStages::qualify_types(ScopeManager *, CompilerMetaData *) -> void {
}


auto spp::asts::mixins::CompilerStages::load_super_scopes(ScopeManager *, CompilerMetaData *) -> void {
}


auto spp::asts::mixins::CompilerStages::pre_analyse_semantics(ScopeManager *, CompilerMetaData *) -> void {
}


auto spp::asts::mixins::CompilerStages::analyse_semantics(ScopeManager *, CompilerMetaData *) -> void {
}


auto spp::asts::mixins::CompilerStages::check_memory(ScopeManager *, CompilerMetaData *) -> void {
}


auto spp::asts::mixins::CompilerStages::code_gen() -> void {
}
