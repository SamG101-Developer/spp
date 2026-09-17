module;
#include <spp/macros.hpp>

module spp.asts.module_implementation_ast;
import spp.asts.module_member_ast;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_ctx;
import genex;

SPP_MOD_BEGIN
ModuleImplementationAst::ModuleImplementationAst(
  decltype(Members) &&members) :
  Members(std::move(members)) {
}

ModuleImplementationAst::~ModuleImplementationAst() = default;

auto ModuleImplementationAst::PosStart() const -> std::size_t {
  // Use the first member.
  return Members.IsEmpty() ? 0 : Members.Front()->PosStart();
}

auto ModuleImplementationAst::PosEnd() const -> std::size_t {
  // Use the last member.
  return Members.IsEmpty() ? 0 : Members.Back()->PosEnd();
}

auto ModuleImplementationAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<ModuleImplementationAst>(
    AstCloneVec(Members));
}

auto ModuleImplementationAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_EXTEND(Members, "\n");
  SPP_STRING_END;
}

auto ModuleImplementationAst::Stage1_PreProcess(
  Ast *ctx) -> void {
  // Shift to members (copy because function pre-processing
  // edits this module's member).
  const auto members_ptrs = Members | genex::views::ptr | genex::to<Vec>();
  for (auto *member : members_ptrs) { member->Stage1_PreProcess(ctx); }
}

auto ModuleImplementationAst::Stage2_GenTopLvlScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Shift to members.
  for (auto const &member : Members) { member->Stage2_GenTopLvlScopes(sm, meta); }
}

auto ModuleImplementationAst::Stage3_GenTopLvlAliases(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Shift to members.
  for (auto const &member : Members) { member->Stage3_GenTopLvlAliases(sm, meta); }
}

auto ModuleImplementationAst::Stage4_ResolveDeclarations(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Shift to members.
  for (auto const &member : Members) { member->Stage4_ResolveDeclarations(sm, meta); }
}

auto ModuleImplementationAst::Stage5_LoadSupScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Shift to members.
  for (auto const &member : Members) { member->Stage5_LoadSupScopes(sm, meta); }
}

auto ModuleImplementationAst::Stage6_PreAnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Shift to members.
  for (auto const &member : Members) { member->Stage6_PreAnalyseSemantics(sm, meta); }
}

auto ModuleImplementationAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Shift to members.
  for (auto const &member : Members) { member->Stage7_AnalyseSemantics(sm, meta); }
}

auto ModuleImplementationAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Shift to members.
  for (auto const &member : Members) { member->Stage8_CheckMemory(sm, meta); }
}

auto ModuleImplementationAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Shift to members, and return nullptr as this value is never used.
  for (auto const &member : Members) { member->Stage9_CompTimeResolve(sm, meta); }
}

auto ModuleImplementationAst::Stage10_PreCodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Shift to members.
  for (auto const &member : Members) { member->Stage10_PreCodeGen(sm, meta, ctx); }
  return nullptr;
}

auto ModuleImplementationAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Shift to members.
  for (auto const &member : Members) { member->Stage11_CodeGen(sm, meta, ctx); }
  return nullptr;
}

SPP_MOD_END
