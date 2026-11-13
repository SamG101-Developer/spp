module;
#include <spp/macros.hpp>

module spp.compiler.compiler;

import spp.analyse.errors.diagnostic_sink;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.scope_tree;
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.monomorphization;
import spp.analyse.utils.type_resolution;
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
import spp.utils.ice;
import spp.utils.progress;
import spp.utils.stats;
import genex;
import std;

namespace spp::compiler {
  /// A stage's progress label as a phase name for the stats report: "Generating
  /// Code (2)....... " is "generating-code-2".
  static auto PhaseName(const spp::StrView label) -> spp::Str;

  /// The counters that mirror what the analysis keeps itself: how many
  /// identities are interned, and how often each cache generation has moved.
  static auto SampleAnalysisCounters() -> void;
}

auto spp::compiler::PhaseName(const spp::StrView label) -> spp::Str {
  auto out = spp::Str();
  for (const auto c : label) {
    if (c == '.' or c == '(' or c == ')') { continue; }
    if (c == ' ') {
      if (not out.empty() and out.back() != '-') { out += '-'; }
      continue;
    }
    out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  while (not out.empty() and out.back() == '-') { out.pop_back(); }
  return out;
}

auto spp::compiler::SampleAnalysisCounters() -> void {
  using spp::utils::stats::Counter;
  using spp::utils::stats::Set;
  Set(Counter::TypeKeysDistinct, spp::analyse::scopes::InternedTypeKeys().size());
  Set(Counter::CompKeysDistinct, spp::analyse::scopes::InternedCompKeys().size());
  Set(Counter::ScopeLinkageGeneration, spp::analyse::scopes::ScopeLinkageGeneration());
  Set(Counter::TypeStructureGeneration, spp::analyse::scopes::TypeStructureGeneration());
}

SPP_MOD_BEGIN
/// [CHECKED]
spp::compiler::Compiler::Compiler(
  const BuildMode mode, const BuildType build_type, TestScope const &tests) :
  _Modules(MakeUnique<ModuleTree>(
    std::filesystem::current_path(), mode, tests)),
  _Mode(mode),
  _BuildType(build_type) {
  _Boot = MakeUnique<CompilerBoot>();
  _Boot->Mode = mode;
}

/// [CHECKED]
auto spp::compiler::Compiler::ForCppGoogleTest(
  const BuildMode mode, Str &&main_code) -> Unique<Compiler> {
  auto c = Unique<Compiler>(new Compiler());
  // Use the special module tree construction for the C++ unit tests, which
  // handles the main.spp code update.
  c->_Modules = ModuleTree::ForCppGoogleTest(
    std::filesystem::current_path(), mode, std::move(main_code));

  // Fill in the rest of the information, including the forced EXE build type as
  // the tests need to run as an executable themself.
  c->_Mode = mode;
  c->_BuildType = BuildType::EXE;
  c->_Boot = MakeUnique<CompilerBoot>();
  c->_Boot->Mode = mode;
  c->_ForCppGoogleTest = true;

  // No codegen needs to be linked together, only verified. This is because the
  // codegen tests are run from shell scripts rather than the unit tests.
  c->_Boot->VerifyOnly = true;
  return c;
}

/// [CHECKED]
spp::compiler::Compiler::~Compiler() {
  // Free this compile, in the order the members would go, and only then the
  // identities it interned: they hold its symbols by address, so a later
  // compile reusing an address would read this one's. Nothing holds a "TypeId"
  // or "CompId" by now.
  _ScopeManager.reset();
  _ScopeTree.reset();
  _Boot.reset();
  _Modules.reset();
  analyse::scopes::ClearSubstitutionMemos();
  analyse::scopes::ClearTypeIds();
  analyse::scopes::ClearCompIds();
}

/// [CHECKED]
auto spp::compiler::Compiler::Compile() -> bool {
  // The global scope is anchored to the first module in the tree, and every
  // stage below walks that tree, so an empty one has nothing to compile and
  // nowhere to put it. Error here to prevent empty vector reads later on.
  if (_Modules->GetModules().empty()) {
    std::cerr
      << "Error: No modules found. A project needs at"
      << "least one '.spp' file under 'src'.\n";
    return false;
  }

  // Before anything is read: a namespace is what says whose a module is.
  if (not _Modules->CheckNamespaces()) { return false; }

  // Determine some useful variables for progress bar visuals and if this
  // compilation is an exe or library; stage 7 has extra analysis for exe entry
  // points.
  const auto is_exe = _BuildType == BuildType::EXE;
  auto num_modules = static_cast<std::uint32_t>(
    _Modules->GetModules().size());

  // One bar at a time: each is created as its stage begins and torn down as the
  // next one replaces it, because a bar animates itself and only one of them
  // can own the terminal line.
  auto stage_name = kCompilerStageNames.begin();
  auto bar = Unique<utils::ProgressBar>();
  utils::stats::SetSampler(SampleAnalysisCounters);
  const auto next_bar = [&]() -> utils::ProgressBar& {
    utils::stats::BeginPhase(PhaseName(*stage_name));
    bar = MakeUnique<utils::ProgressBar>(
      *stage_name++, num_modules, not _ForCppGoogleTest);
    return *bar;
  };

  // Whether the backend produced what it set out to; every stage before it
  // reports a rejection by throwing.
  auto built = false;

  // We need the cleanup on error for the test suite runs (parallel), but in
  // debug it's one shot, and error checking needs the full stack trace.
#if !SPP_DEBUG
  try {
#endif
    // Whatever a previous compile in this process recovered from, or resolved,
    // is not this compile's.
    analyse::errors::diagnostic_sink::Clear();
    lsp::resolution_index::Clear();

    // Run the first two non-analysis-oriented stages: lexing and parsing. This
    // builds the token sets and ast trees for each module, forming the base of
    // what gets analysed.
    _Boot->Lex(next_bar(), *_Modules);
    _Boot->Parse(next_bar(), *_Modules);
    _TestNames = _Boot->TestNames;

    // Build the new scope tree and scope manager, using a new global scope for
    // this compilation. Initialise the precompiled types.
    _ScopeManager = nullptr;
    _ScopeTree = MakeUnique<analyse::scopes::ScopeTree>(
      analyse::scopes::Scope::NewGlobal(*_Modules->GetModules()[0]));
    _ScopeManager = MakeUnique<analyse::scopes::ScopeManager>(
      analyse::scopes::ScopeManager::ForTree(*_ScopeTree));
    asts::generate::common_types_precompiled::InitTypes();

    /// Run the first three stages, which pre-process the asts, generate type
    /// scopes, and then the type-alias scopes. At this point, all core type
    /// information (non-monomorphized) is known.
    _Boot->Stage1_PreProcess(
      next_bar(), *_Modules, nullptr);
    _Boot->Stage2_GenTopLvlScopes(
      next_bar(), *_Modules, _ScopeManager.get());
    _Boot->Stage3_GenTopLvlAliases(
      next_bar(), *_Modules, _ScopeManager.get());
    analyse::utils::type_resolution::StampPrecompiledTypes(
      *_ScopeManager->GlobalScope());

    // Run all the future analysis stages, all the way through to the memory
    // checker. After this stage, no more semantic errors can be raised, and
    // provided bug-free codegen, IR should execute safely.
    _Boot->Stage4_ResolveDeclarations(
      next_bar(), *_Modules, _ScopeManager.get());
    _Boot->Stage5_LoadSupScopes(
      next_bar(), *_Modules, _ScopeManager.get());
    _Boot->Stage5_5_AttachSupScopes(
      next_bar(), _ScopeManager.get());
    _Boot->Stage6_PreAnalyseSemantics(
      next_bar(), *_Modules, _ScopeManager.get());
    _Boot->Stage7_AnalyseSemantics(
      next_bar(), *_Modules, is_exe, _ScopeManager.get());
    _Boot->Stage8_CheckMemory(
      next_bar(), *_Modules, _ScopeManager.get());
    CollectCompTimeConstants();

    // Builds that are "analyse only" (IDEA indexing), or have errors, stop
    // after stage 8.
    if (_AnalyseOnly or analyse::errors::diagnostic_sink::HasErrors()) {
      built = not analyse::errors::diagnostic_sink::HasErrors();
    }

    // Otherwise, we can finish the compilation for IR, by running the full
    // monomorphization engine, and the codegen. Successful codegen is
    // determined by the state of the LTO.
    else {
      _Boot->Stage9_5_Monomorphise(
        next_bar(), *_Modules, _ScopeManager.get());
      _Boot->Stage10_PreCodeGen(
        next_bar(), *_Modules, _ScopeManager.get());
      built = _Boot->Stage11_CodeGen(
        next_bar(), *_Modules, _ScopeManager.get(),
        _Mode == BuildMode::REL ? 3u : 0u);
    }
#if !SPP_DEBUG
  }
  catch (...) {
    // Clear globals while the scope tree is still alive (so precompiled types
    // release before their scopes are freed), then re-throw to the caller.
    Cleanup();
    throw;
  }
#endif
  Cleanup();
  return built;
}

/// [CHECKED]
auto spp::compiler::Compiler::SetTestFilters(
  Str name_filter, Str group_filter) const -> void {
  // Set the test name and test group filters, meaning that the test run will
  // constrain whilst S++ unit tests are run by "spp test".
  _Boot->TestNameFilter = std::move(name_filter);
  _Boot->TestGroupFilter = std::move(group_filter);
}

/// [CHECKED]
auto spp::compiler::Compiler::SetAnalyseOnly(
  const bool analyse_only) -> void {
  /// Set the flag that prevents monomorphization and codegen from running, as
  /// some places don't need this (IDEA indexing).
  _AnalyseOnly = analyse_only;
}

/// [CHECKED]
auto spp::compiler::Compiler::TestCount() const -> std::size_t {
  // The test count if the number of tests present, determined by the length of
  // the test names list.
  return _TestNames.size();
}

/// [CHECKED]
auto spp::compiler::Compiler::TestNames() const -> Vec<Str> const& {
  // Getter for the test names, which are filled by the compiler boot scanning
  // the test folder(s)
  return _TestNames;
}

/// [CHECKED]
auto spp::compiler::Compiler::CollectCompTimeConstants() -> void {
  // Failsafe against a nullptr scope manager, which prevents any cmp constants
  // from being read.
  if (_ScopeManager == nullptr) {
    Ice("comp-time constants collected with no scope manager");
  }

  // A "cmp" outside the main module belongs to a dependency, and modules are
  // not held in any particular order, so the main module is found by its path
  // rather than by position.
  const auto main_path = _Modules->RootPath() / "src" / "main.spp";
  const auto modules = _Modules->GetModules();
  const auto main_module = genex::find_if(
    modules, [&](const auto mod) {
      return mod->Path == main_path and mod->ModuleAst != nullptr;
    });
  if (main_module == modules.end()) { return; }

  // Comp-time resolution replaces a "cmp" statement's value with the literal it
  // resolved to, so the module's own ast is the record of what was computed.
  for (const auto member : asts::AstBody((*main_module)->ModuleAst.get())) {
    const auto cmp = member->To<asts::CmpStatementAst>();
    if (cmp == nullptr or cmp->Value == nullptr) { continue; }

    // Don't keep "use" aliased constants, or compiler-generated mock function
    // values like from "cmp func = $func()".
    if (cmp->IsFromUseStatement()) { continue; }
    if (cmp->Type->IsCompilerGeneratedType()) { continue; }
    _CompTimeConstants[cmp->Name->Val] = cmp->Value->ToString();
  }
}

/// [CHECKED]
auto spp::compiler::Compiler::CompTimeConstants() const
  -> Map<Str, Str> const& {
  // Getter for the compile time constants which is empty until filled by the
  // collection function.
  return _CompTimeConstants;
}

/// [CHECKED]
auto spp::compiler::Compiler::Cleanup() -> void {
  // The compiler's cleanup steps clears the common precompiled type's static
  // list, and the monomorphization's instantiation cache.
  asts::generate::common_types_precompiled::ClearTypes();
  analyse::utils::monomorphization::ClearInstantiations();
}

SPP_MOD_END
