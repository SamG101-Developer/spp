module;
#include <spp/macros.hpp>

module spp.asts.module_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.module_implementation_ast;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_ctx;
import spp.utils.files;
import genex;

use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);
use_alias(spp::compiler, CompilerBoot);

SPP_MOD_BEGIN
spp::asts::ModulePrototypeAst::ModulePrototypeAst(
  decltype(Impl) &&impl) :
  Impl(std::move(impl)) {
}

spp::asts::ModulePrototypeAst::~ModulePrototypeAst() = default;

auto spp::asts::ModulePrototypeAst::PosStart() const -> std::size_t {
  // Use the impl.
  return Impl->PosStart();
}

auto spp::asts::ModulePrototypeAst::PosEnd() const -> std::size_t {
  // Use the impl.
  return Impl->PosEnd();
}

auto spp::asts::ModulePrototypeAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<ModulePrototypeAst>(
    AstClone(Impl));
}

auto spp::asts::ModulePrototypeAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Impl);
  SPP_STRING_END;
}

auto spp::asts::ModulePrototypeAst::Stage1_PreProcess(
  Ast *ctx) -> void {
  // Shift to implementation.
  Ast::Stage1_PreProcess(ctx);
  Impl->Stage1_PreProcess(this);
}

auto spp::asts::ModulePrototypeAst::Stage2_GenTopLvlScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Shift to implementation.
  Impl->Stage2_GenTopLvlScopes(sm, meta);
}

auto spp::asts::ModulePrototypeAst::Stage3_GenTopLvlAliases(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Shift to implementation.
  Impl->Stage3_GenTopLvlAliases(sm, meta);
}

auto spp::asts::ModulePrototypeAst::Stage4_ResolveDeclarations(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Shift to implementation.
  Impl->Stage4_ResolveDeclarations(sm, meta);
}

auto spp::asts::ModulePrototypeAst::Stage5_LoadSupScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Shift to implementation.
  Impl->Stage5_LoadSupScopes(sm, meta);
}

auto spp::asts::ModulePrototypeAst::Stage6_PreAnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Shift to implementation.
  Impl->Stage6_PreAnalyseSemantics(sm, meta);
}

auto spp::asts::ModulePrototypeAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Shift to implementation.
  Impl->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::ModulePrototypeAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Shift to implementation.
  Impl->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::ModulePrototypeAst::Stage10_PreCodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Shift to implementation.
  return Impl->Stage10_PreCodeGen(sm, meta, ctx);
}

auto spp::asts::ModulePrototypeAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Shift to implementation.
  return Impl->Stage11_CodeGen(sm, meta, ctx);
}

SPP_MOD_END
