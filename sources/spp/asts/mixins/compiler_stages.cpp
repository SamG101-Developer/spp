module;
#include <spp/analyse/macros.hpp>

module spp.asts.mixins.compiler_stages;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.asts.ast;
import spp.codegen.llvm_ctx;


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


auto spp::asts::mixins::CompilerStages::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    raise<analyse::errors::SppInvalidComptimeOperationError>(
        {sm->current_scope}, ERR_ARGS(dynamic_cast<Ast&>(*this)));
    std::unreachable();
}


auto spp::asts::mixins::CompilerStages::stage_10_code_gen_1(
    ScopeManager *,
    CompilerMetaData *,
    codegen::LLvmCtx *)
    -> llvm::Value* {
    return nullptr;
}


auto spp::asts::mixins::CompilerStages::stage_11_code_gen_2(
    ScopeManager *,
    CompilerMetaData *,
    codegen::LLvmCtx *)
    -> llvm::Value* {
    return nullptr;
}
