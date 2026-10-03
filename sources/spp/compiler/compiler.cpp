module;
#include <spp/macros.hpp>

module spp.compiler.compiler;

import spp.analyse.errors.diagnostic_sink;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.monomorphization;
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
import spp.lsp.resolution_index;
import spp.utils.progress;
import genex;
import std;

SPP_MOD_BEGIN
auto spp::compiler::Compiler::ModeName(
  const Mode mode)
  -> Str {
  return mode == Mode::REL ? "rel" : "dev";
}

spp::compiler::Compiler::Compiler(
  const Mode mode,
  const BuildType build_type,
  TestScope const &tests) :
  _Modules(MakeUnique<ModuleTree>(std::filesystem::current_path(), ModeName(mode), tests)),
  _Mode(mode),
  _BuildType(build_type) {
  _Path = std::filesystem::current_path() / "src";
  _Boot = MakeUnique<CompilerBoot>();
}

auto spp::compiler::Compiler::ForCppGoogleTest(
  const Mode mode,
  Str &&main_code)
  -> Unique<Compiler> {
  auto c = Unique<Compiler>(new Compiler());
  c->_Modules = ModuleTree::ForCppGoogleTest(
    std::filesystem::current_path(), ModeName(mode), std::move(main_code));
  c->_Mode = mode;
  c->_BuildType = BuildType::EXE; // Tests for "main" in the test suite.
  c->_Path = std::filesystem::current_path() / "src";
  c->_Boot = MakeUnique<CompilerBoot>();
  c->_Boot->VerifyOnly = true;
  c->_ForCppGoogleTest = true;
  return c;
}

spp::compiler::Compiler::~Compiler() = default;

auto spp::compiler::Compiler::Compile() -> bool {
  // The global scope is anchored to the first module in the
  // tree, and every stage below walks that tree, so an empty
  // one has nothing to compile and nowhere to put it. Error
  // here to prevent empty vector reads later on.
  if (_Modules->GetModules().IsEmpty()) {
    std::cerr
      << "Error: No modules found. A project needs at least one '.spp' file under 'src'.\n";
    return false;
  }

  const auto is_exe = _BuildType == BuildType::EXE;
  auto num_modules = static_cast<std::uint32_t>(
    _Modules->GetModules().Len());

  // One bar at a time: each is created as its stage begins and
  // torn down as the next one replaces it, because a bar animates
  // itself and only one of them can own the terminal line.
  auto stage_name = kCompilerStageNames.begin();
  auto bar = Unique<utils::ProgressBar>();
  const auto next_bar = [&]() -> utils::ProgressBar& {
    bar = MakeUnique<utils::ProgressBar>(
      *stage_name++, num_modules, not _ForCppGoogleTest);
    return *bar;
  };

  // Whether the back end produced what it set out to; every
  // stage before it reports a rejection by throwing.
  auto built = false;

  // We need the cleanup on error for the test suite runs
  // (parallel), but in debug it's one shot, and error checking
  // needs the full stack trace.
#if !SPP_DEBUG
  try {
#endif
    // Whatever a previous compile in this process recovered
    // from, or resolved, is not this compile's.
    analyse::errors::diagnostic_sink::Clear();
    lsp::resolution_index::Clear();

    _Boot->Lex(next_bar(), *_Modules);
    _Boot->Parse(next_bar(), *_Modules);
    _TestCount = _Boot->TestCount;
    _TestNames = _Boot->TestNames;
    _ScopeManager = MakeUnique<analyse::scopes::ScopeManager>(
      analyse::scopes::Scope::NewGlobal(*_Modules->GetModules()[0]), nullptr);
    asts::generate::common_types_precompiled::InitTypes();

    _Boot->Stage1_PreProcess(next_bar(), *_Modules, nullptr);
    _Boot->Stage2_GenTopLvlScopes(next_bar(), *_Modules, _ScopeManager.get());
    _Boot->Stage3_GenTopLvlAliases(next_bar(), *_Modules, _ScopeManager.get());
    _Boot->Stage4_ResolveDeclarations(next_bar(), *_Modules, _ScopeManager.get());
    _Boot->Stage5_LoadSupScopes(next_bar(), *_Modules, _ScopeManager.get());
    _Boot->Stage5_5_AttachSupScopes(next_bar(), _ScopeManager.get());
    _Boot->Stage6_PreAnalyseSemantics(next_bar(), *_Modules, _ScopeManager.get());
    _Boot->Stage7_AnalyseSemantics(next_bar(), *_Modules, is_exe, _ScopeManager.get());
    _Boot->Stage8_CheckMemory(next_bar(), *_Modules, _ScopeManager.get());
    _Boot->Stage9_CompTimeResolve(next_bar(), *_Modules, _ScopeManager.get());
    CollectCompTimeConstants();

    // Builds that are "analyse only" (IDEA indexing), can stop
    // after stage 9.
    if (_AnalyseOnly or analyse::errors::diagnostic_sink::HasErrors()) {
      built = not analyse::errors::diagnostic_sink::HasErrors();
    }
    else {
      _Boot->Stage9_5_Monomorphise(next_bar(), *_Modules, _ScopeManager.get());
      _Boot->Stage10_PreCodeGen(next_bar(), *_Modules, _ScopeManager.get());
      built = _Boot->Stage11_CodeGen(next_bar(), *_Modules, _ScopeManager.get(), _Mode == Mode::REL ? 3u : 0u);
    }
#if !SPP_DEBUG
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
  return built;
}

auto spp::compiler::Compiler::SetTestFilters(
  Str name_filter,
  Str group_filter) const
  -> void {
  _Boot->TestNameFilter = std::move(name_filter);
  _Boot->TestGroupFilter = std::move(group_filter);
}

auto spp::compiler::Compiler::SetAnalyseOnly(
  const bool analyse_only)
  -> void {
  _AnalyseOnly = analyse_only;
}

auto spp::compiler::Compiler::TestCount() const
  -> std::size_t {
  return _TestCount;
}

auto spp::compiler::Compiler::TestNames() const
  -> Vec<Str> const& {
  return _TestNames;
}

auto spp::compiler::Compiler::CollectCompTimeConstants() -> void {
  // A "cmp" outside the main module belongs to a dependency,
  // and modules are not held in any particular order, so the
  // main module is found by its path rather than by position.
  if (_ScopeManager == nullptr) { return; }

  const auto main_path = _Modules->RootPath() / "src" / "main.spp";
  const auto modules = _Modules->GetModules();
  const auto main_module = genex::find_if(
    modules, [&](auto const *mod) { return mod->Path == main_path and mod->ModuleAst != nullptr; });
  if (main_module == modules.end()) { return; }

  // Comp-time resolution replaces a "cmp" statement's value
  // with the literal it resolved to, so the module's own ast
  // is the record of what was computed - and it lists the
  // constants the module declares, where the module's scope
  // would also hold everything the prelude imported into it.
  for (auto const *member : asts::AstBody((*main_module)->ModuleAst.get())) {
    const auto *cmp = member->To<asts::CmpStatementAst>();
    if (cmp == nullptr or cmp->Value == nullptr) { continue; }

    // A "use"-generated constant aliases another module's,
    // and a compiler-generated type is a mock standing in for
    // a function rather than a value that was written.
    if (cmp->IsFromUseStatement() or cmp->Type->IsCompilerGeneratedType()) { continue; }
    _CompTimeConstants[cmp->Name->Val] = cmp->Value->ToString();
  }
}

auto spp::compiler::Compiler::CompTimeConstants() const
  -> Map<Str, Str> const& {
  return _CompTimeConstants;
}

auto spp::compiler::Compiler::Cleanup() -> void {
  asts::generate::common_types_precompiled::ClearTypes();
  analyse::scopes::ScopeManager::Cleanup();
  analyse::utils::monomorphization::ClearInstantiations();
}

SPP_MOD_END
