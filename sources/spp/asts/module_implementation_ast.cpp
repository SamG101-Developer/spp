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
import spp.asts.sup_prototype_extension_ast;
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
  /// store the error in the sink against the member, which
  /// "poisons" it so every later stage skips it, allowing
  /// multiple errors, from different members, to show at once.
  /// Answers whether the member ran without an error.
  template <typename F>
  auto CheckMember(Ast *const member, F &&check) -> bool {
    IMPORT_UTILS;
    namespace sink = diagnostic_sink;
    // Normal behaviour: run the member, an error stops execution,
    // displays the error and terminates the compiler.
    if (not sink::IsEnabled()) {
      check(member);
      return true;
    }

    // A member that an earlier stage "poisoned" (via the "report"
    // method) is not run again.
    if (sink::IsPoisoned(member)) { return false; }

    // Otherwise, try to run the check for the member, and if it
    // raises an error, then report it to the sink.
    try {
      check(member);
      return true;
    }
    catch (SemanticError const &e) {
      sink::Report(e, member);
      return false;
    }
  }

  /// Whether a member has scopes of its own for the walk to skip.
  /// A member's scope is where it was declared unless it makes
  /// one, and skipping past the scope the walk is already in (or
  /// one above it) would throw away the rest of the module.
  auto OwnsScope(Ast const &member, Scope const &walk_scope) -> bool {
    for (auto const *s = &walk_scope; s != nullptr; s = s->Parent) {
      if (s == member.GetAstScope()) { return false; }
    }
    return member.GetAstScope() != nullptr;
  }

  /// "CheckMember" for a stage that walks the scopes: a member
  /// that did not run to the end is skipped past, so the walk
  /// continues at the next member. The walk only moves forwards,
  /// and an error can be raised after the member has walked past
  /// its own scopes (a value left unconsumed is found as the
  /// function's scope is left), so it is first put back where it
  /// stood before the member, from where the member's scopes are
  /// all still ahead.
  template <typename F>
  auto RunMember(Ast *const member, ScopeManager *const sm, F &&stage) -> void {
    const auto start_scope = sm->CurrentScope;
    const auto start_it = sm->GetCurrentIterator();
    if (not CheckMember(member, std::forward<F>(stage))) {
      sm->Reset(start_scope, start_it);
      if (OwnsScope(*member, *start_scope)) { sm->SkipPastScope(member->GetAstScope()); }
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
  for (auto const &member : Members) {
    RunMember(member.get(), sm, [&](auto *m) { m->Stage6_PreAnalyseSemantics(sm, meta); });
  }
}

auto ModuleImplementationAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Shift to members.
  for (auto const &member : Members) {
    RunMember(member.get(), sm, [&](auto *m) { m->Stage7_AnalyseSemantics(sm, meta); });
  }

  // What the module itself holds - its own declarations,
  // and everything imported into it - which can be named
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

auto ModuleImplementationAst::CheckExtensionMembers(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Check the members of each extension block against its super
  // class. Nothing walks the scopes here, so there is nothing to
  // skip past for a block that fails.
  for (auto const &member : Members) {
    if (const auto ext = member->To<SupPrototypeExtensionAst>()) {
      CheckMember(member.get(), [&](auto *) { ext->CheckExtensionMembers(*sm, meta); });
    }
  }
}

SPP_MOD_END
