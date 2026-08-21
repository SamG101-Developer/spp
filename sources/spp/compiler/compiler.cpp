module;
#include <spp/macros.hpp>

module spp.compiler.compiler;

import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.instantiation_queue;
import spp.asts.ast;
import spp.asts.cmp_statement_ast;
import spp.asts.identifier_ast;
import spp.asts.module_prototype_ast;
import spp.asts.type_ast;
import spp.asts.type_statement_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;
import spp.compiler.compiler_boot;
import spp.compiler.module_tree;
import spp.lex.tokens;
import spp.utils.progress;
import genex;
import std;

SPP_MOD_BEGIN
spp::compiler::Compiler::Compiler(
  const Mode mode,
  const BuildType build_type) :
  m_modules(MakeUnique<ModuleTree>(std::filesystem::current_path())),
  m_mode(mode),
  m_build_type(build_type) {
  m_path = std::filesystem::current_path() / "src";
  m_boot = MakeUnique<CompilerBoot>();
}

auto spp::compiler::Compiler::ForUnitTests(
  const Mode mode,
  Str &&main_code)
  -> Unique<Compiler> {
  auto c = MakeUnique<Compiler>();
  c->m_modules = ModuleTree::ForUnitTests(std::filesystem::current_path(), std::move(main_code));
  c->m_mode = mode;
  c->m_build_type = BuildType::EXE; // Tests for "main" in the test suite.
  c->m_path = std::filesystem::current_path() / "src";
  c->m_boot = MakeUnique<CompilerBoot>();
  c->m_for_unit_tests = true;
  return c;
}

spp::compiler::Compiler::~Compiler() = default;

auto spp::compiler::Compiler::Compile() -> void {
  const auto is_exe = m_build_type == BuildType::EXE;
  auto progress_bars = Vec<Unique<utils::ProgressBar>>();
  auto num_modules = static_cast<std::uint32_t>(m_modules->GetModules().Len());
  for (auto stage : kCompilerStageNames) {
    auto p = MakeUnique<utils::ProgressBar>(stage, num_modules, not m_for_unit_tests);
    progress_bars.EmplaceBack(std::move(p));
  }

  // We need the cleanup on error for the test suite runs (parallel), but in debug it's one shot, and error checking
  // needs the full stack trace.
  auto ps = progress_bars.begin();
#ifdef NDEBUG
  try {
#endif
    m_boot->Lex(**ps++, *m_modules);
    m_boot->Parse(**ps++, *m_modules);
    m_scope_manager = MakeUnique<analyse::scopes::ScopeManager>(
      analyse::scopes::Scope::NewGlobal(*m_modules->GetModules()[0]), nullptr);
    asts::generate::common_types_precompiled::InitTypes();

    m_boot->Stage1_PreProcess(**ps++, *m_modules, nullptr);
    m_boot->Stage2_GenTopLvlScopes(**ps++, *m_modules, m_scope_manager.get());
    m_boot->Stage3_GenTopLvlAliases(**ps++, *m_modules, m_scope_manager.get());
    m_boot->Stage4_QualifyTypes(**ps++, *m_modules, m_scope_manager.get());
    m_boot->Stage5_LoadSupScopes(**ps++, *m_modules, m_scope_manager.get());
    m_boot->Stage6_PreAnalyseSemantics(**ps++, *m_modules, m_scope_manager.get());
    m_boot->Stage7_AnalyseSemantics(**ps++, *m_modules, is_exe, m_scope_manager.get());
    m_boot->Stage8_CheckMemory(**ps++, *m_modules, m_scope_manager.get());
    m_boot->Stage9_CompTimeResolve(**ps++, *m_modules, m_scope_manager.get());
    CollectCompTimeConstants();
    if (not m_for_unit_tests) {
      m_boot->Stage9_5_Monomorphise(**ps++, *m_modules, m_scope_manager.get());
      m_boot->Stage10_PreCodeGen(**ps++, *m_modules, m_scope_manager.get());
      m_boot->Stage11_CodeGen(**ps++, *m_modules, m_scope_manager.get(), m_mode == Mode::REL, m_mode == Mode::REL);
    }
#ifdef NDEBUG
  }
  catch (...) {
    // Clear globals while the scope tree is still alive
    // (so precompiled types release before their scopes
    // are freed), then re-throw to the caller.
    Cleanup();
    throw;
  }
#endif
  Cleanup();
}

auto spp::compiler::Compiler::CollectCompTimeConstants() -> void {
  // A "cmp" outside the main module belongs to a dependency,
  // and modules are not held in any particular order, so the
  // main module is found by its path rather than by position.
  if (m_scope_manager == nullptr) { return; }

  const auto main_path = m_modules->RootPath() / "src" / "main.spp";
  const auto modules = m_modules->GetModules();
  const auto main_module = genex::find_if(
    modules, [&](auto const *mod) { return mod->path == main_path and mod->module_ast != nullptr; });
  if (main_module == modules.end()) { return; }

  // Comp-time resolution replaces a "cmp" statement's value with the literal it resolved to, so the module's own ast
  // is the record of what was computed - and it lists exactly the constants the module declares, where the module's
  // scope would also hold everything the prelude imported into it.
  for (auto const *member : asts::AstBody((*main_module)->module_ast.get())) {
    const auto *cmp = member->To<asts::CmpStatementAst>();
    if (cmp == nullptr or cmp->Value == nullptr) { continue; }

    // A "use"-generated constant aliases another module's, and a compiler-generated type is a mock standing in for a
    // function rather than a value that was written.
    if (cmp->IsFromUseStatement() or cmp->Type->IsCompilerGeneratedType()) { continue; }
    m_comp_time_constants[cmp->Name->Val] = cmp->Value->ToString();
  }
}

auto spp::compiler::Compiler::CompTimeConstants() const
  -> Map<Str, Str> const& {
  return m_comp_time_constants;
}

auto spp::compiler::Compiler::Cleanup() -> void {
  asts::generate::common_types_precompiled::ClearTypes();
  analyse::scopes::ScopeManager::Cleanup();
  analyse::utils::instantiation_queue::Clear();
}

SPP_MOD_END
