module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.module_implementation_ast;
import spp.analyse.errors.diagnostic_sink;
import spp.analyse.errors.semantic_error;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.asts.ast;
import spp.asts.module_member_ast;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_ctx;
import spp.lsp.resolution_index;
import genex;

use_ns(spp::analyse::utils);

SPP_MOD_BEGIN
namespace {
  /// For the normal caller, once we get an error, we raise it
  /// and stop the compiler execution. If the sink is enabled
  /// however, this is for the IDEA plugin's indexing, so we
  /// store the error in the sink, shift past the member's scope,
  /// and move onto the next member, allowing multiple errors,
  /// from different functions, to show at once.
  template <typename F>
  auto RunMember(Ast *const member, ScopeManager *const sm, F &&stage) -> void {
    IMPORT_UTILS;
    namespace sink = diagnostic_sink;
    // Normal behaviour: run the member, an error stops execution,
    // displays the error and terminates the compiler.
    if (not sink::IsEnabled()) { return stage(member); }

    // If we have "poisoned" the sink for this member (via the
    // "report" method), then skip the scopes and we move to the
    // next member.
    if (sink::IsPoisoned(member)) {
      sm->SkipPastScope(member->GetAstScope());
      return;
    }

    // Otherwise, try to run the stage for the member, and if it
    // raises an error, then report it to the sink, skip the member's
    // scopes, and move to the next member.
    try {
      stage(member);
    }
    catch (SemanticError const &e) {
      sink::Report(e, member);
      sm->SkipPastScope(member->GetAstScope());
    }
  }
}

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
  for (auto const &member : Members) {
    RunMember(member.get(), sm, [&](auto *m) { m->Stage7_AnalyseSemantics(sm, meta); });
  }

  // What the module itself holds - its own declarations, and everything imported into it - which can be named
  // anywhere in the file.
  if (lsp::resolution_index::IsEnabled()) {
    lsp::resolution_index::RecordScopeOf(*this, *sm, true);
  }
}

auto ModuleImplementationAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Shift to members.
  for (auto const &member : Members) {
    RunMember(member.get(), sm, [&](auto *m) { m->Stage8_CheckMemory(sm, meta); });
  }
}

auto ModuleImplementationAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Shift to members, and return nullptr as this value is never used.
  for (auto const &member : Members) {
    RunMember(member.get(), sm, [&](auto *m) { m->Stage9_CompTimeResolve(sm, meta); });
  }
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
