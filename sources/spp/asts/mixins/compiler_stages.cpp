module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.mixins.compiler_stages;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.asts.ast;
import spp.codegen.llvm_ctx;

SPP_MOD_BEGIN
CompilerStages::CompilerStages() = default;

CompilerStages::~CompilerStages() = default;

auto CompilerStages::Stage1_PreProcess(
  Ast *)
  -> void {
  // Default behaviour: no actions.
}

auto CompilerStages::Stage2_GenTopLvlScopes(
  ScopeManager *,
  CompilerMetaData *)
  -> void {
  // Default behaviour: no actions.
}

auto CompilerStages::Stage3_GenTopLvlAliases(
  ScopeManager *,
  CompilerMetaData *)
  -> void {
  // Default behaviour: no actions.
}

auto CompilerStages::Stage4_ResolveDeclarations(
  ScopeManager *,
  CompilerMetaData *)
  -> void {
  // Default behaviour: no actions.
}

auto CompilerStages::Stage5_LoadSupScopes(
  ScopeManager *,
  CompilerMetaData *)
  -> void {
  // Default behaviour: no actions.
}

auto CompilerStages::Stage6_PreAnalyseSemantics(
  ScopeManager *,
  CompilerMetaData *)
  -> void {
  // Default behaviour: no actions.
}

auto CompilerStages::Stage7_AnalyseSemantics(
  ScopeManager *,
  CompilerMetaData *)
  -> void {
  // Default behaviour: no actions.
}

auto CompilerStages::Stage8_CheckMemory(
  ScopeManager *,
  CompilerMetaData *)
  -> void {
  // Default behaviour: no actions.
}

auto CompilerStages::Stage9_CompTimeResolve(
  ScopeManager *sm,
  CompilerMetaData *)
  -> void {
  // Default behaviour: this AST does not support
  // comptime resolution, so throw an error.
  using analyse::errors::SppInvalidComptimeOperationError;
  Raise<SppInvalidComptimeOperationError>(
    {sm->CurrentScope}, ERR_ARGS(dynamic_cast<Ast&>(*this)));
}

auto CompilerStages::Stage10_PreCodeGen(
  ScopeManager *,
  CompilerMetaData *,
  LlvmCtx *)
  -> llvm::Value* {
  // Default behaviour: no llvm generation => nullptr value returned,
  return nullptr;
}

auto CompilerStages::Stage11_CodeGen(
  ScopeManager *,
  CompilerMetaData *,
  LlvmCtx *)
  -> llvm::Value* {
  // Default behaviour: no llvm generation => nullptr value returned,
  return nullptr;
}

SPP_MOD_END
