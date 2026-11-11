module;
#include <spp/macros.hpp>

module spp.asts.mixins.compiler_stages;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.asts.ast;
import spp.codegen.llvm_ctx;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, Scope);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

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

auto spp::asts::mixins::CompilerStages::Stage4_ResolveDeclarations(
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

auto spp::asts::mixins::CompilerStages::Stage10_PreCodeGen(
  ScopeManager *,
  CompilerMetaData *,
  LlvmCtx *)
  -> llvm::Value* {
  // Default behaviour: no llvm generation => nullptr value returned,
  return nullptr;
}

auto spp::asts::mixins::CompilerStages::Stage11_CodeGen(
  ScopeManager *,
  CompilerMetaData *,
  LlvmCtx *)
  -> llvm::Value* {
  // Default behaviour: no llvm generation => nullptr value returned,
  return nullptr;
}

SPP_MOD_END
