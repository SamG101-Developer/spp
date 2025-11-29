module spp.asts.mixins.compiler_stages;


spp::asts::mixins::CompilerStages::~CompilerStages() = default;


auto spp::asts::mixins::CompilerStages::stage_1_pre_process(
    Ast *)
    -> void {
}


auto spp::asts::mixins::CompilerStages::stage_2_gen_top_level_scopes(
    ScopeManager *,
    CompilerMetaData *)
    -> void {
}


auto spp::asts::mixins::CompilerStages::stage_3_gen_top_level_aliases(
    ScopeManager *,
    CompilerMetaData *)
    -> void {
}


auto spp::asts::mixins::CompilerStages::stage_4_qualify_types(
    ScopeManager *,
    CompilerMetaData *)
    -> void {
}


auto spp::asts::mixins::CompilerStages::stage_5_load_super_scopes(
    ScopeManager *,
    CompilerMetaData *)
    -> void {
}


auto spp::asts::mixins::CompilerStages::stage_6_pre_analyse_semantics(
    ScopeManager *,
    CompilerMetaData *)
    -> void {
}


auto spp::asts::mixins::CompilerStages::stage_7_analyse_semantics(
    ScopeManager *,
    CompilerMetaData *)
    -> void {
}


auto spp::asts::mixins::CompilerStages::stage_8_check_memory(
    ScopeManager *,
    CompilerMetaData *)
    -> void {
}


auto spp::asts::mixins::CompilerStages::stage_9_code_gen_1(
    ScopeManager *,
    CompilerMetaData *,
    codegen::LLvmCtx *)
    -> llvm::Value* {
    return nullptr;
}


auto spp::asts::mixins::CompilerStages::stage_10_code_gen_2(
    ScopeManager *,
    CompilerMetaData *,
    codegen::LLvmCtx *)
    -> llvm::Value* {
    return nullptr;
}
