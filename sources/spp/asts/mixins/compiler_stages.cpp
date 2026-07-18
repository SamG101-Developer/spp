module;
#include <spp/analyse/macros.hpp>
#include <spp/macros.hpp>

module spp.asts.mixins.compiler_stages;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.asts.ast;
import spp.codegen.llvm_ctx;

SPP_MOD_BEGIN
spp::asts::mixins::CompilerStages::CompilerStages() = default;

spp::asts::mixins::CompilerStages::~CompilerStages() = default;

auto spp::asts::mixins::CompilerStages::Stage1_PreProcess(
    Ast *)
    -> void {
    // Default behaviour: no actions.
}

auto spp::asts::mixins::CompilerStages::Stage2_GenTopLvlScopes(
    ScopeManager *,
    CompilerMetaData *)
    -> void {
    // Default behaviour: no actions.
}

auto spp::asts::mixins::CompilerStages::Stage3_GenTopLvlAliases(
    ScopeManager *,
    CompilerMetaData *)
    -> void {
    // Default behaviour: no actions.
}

auto spp::asts::mixins::CompilerStages::Stage4_QualifyTypes(
    ScopeManager *,
    CompilerMetaData *)
    -> void {
    // Default behaviour: no actions.
}

auto spp::asts::mixins::CompilerStages::Stage5_LoadSupScopes(
    ScopeManager *,
    CompilerMetaData *)
    -> void {
    // Default behaviour: no actions.
}

auto spp::asts::mixins::CompilerStages::Stage6_PreAnalyseSemantics(
    ScopeManager *,
    CompilerMetaData *)
    -> void {
    // Default behaviour: no actions.
}

auto spp::asts::mixins::CompilerStages::Stage7_AnalyseSemantics(
    ScopeManager *,
    CompilerMetaData *)
    -> void {
    // Default behaviour: no actions.
}

auto spp::asts::mixins::CompilerStages::Stage8_CheckMemory(
    ScopeManager *,
    CompilerMetaData *)
    -> void {
    // Default behaviour: no actions.
}

auto spp::asts::mixins::CompilerStages::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    // Default behaviour: this AST does not support comptime resolution, so throw an error.
    using analyse::errors::SppInvalidComptimeOperationError;
    Raise<SppInvalidComptimeOperationError>(
        {sm->CurrentScope}, ERR_ARGS(dynamic_cast<Ast&>(*this)));
}

auto spp::asts::mixins::CompilerStages::Stage10_PreCodeGen(
    ScopeManager *,
    CompilerMetaData *,
    codegen::LLvmCtx *)
    -> llvm::Value* {
    // Default behaviour: no llvm generation => nullptr value returned,
    return nullptr;
}

auto spp::asts::mixins::CompilerStages::Stage11_CodeGen(
    ScopeManager *,
    CompilerMetaData *,
    codegen::LLvmCtx *)
    -> llvm::Value* {
    // Default behaviour: no llvm generation => nullptr value returned,
    return nullptr;
}

SPP_MOD_END
