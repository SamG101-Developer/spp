module;
#include <spp/macros.hpp>

module spp.compiler.compiler;

import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.asts.module_prototype_ast;
import spp.asts.type_statement_ast;
import spp.asts.generate.common_types_precompiled;
import spp.compiler.compiler_boot;
import spp.compiler.module_tree;
import spp.lex.tokens;
import spp.utils.progress;
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

    // Save the modules into the CompilerBoot instance, and lex/parse.
    auto ps = progress_bars.begin();
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
    // m_boot->Stage10_PreCodeGen(**ps++, *m_modules, m_scope_manager.get());
    // m_boot->Stage11_CodeGen(**ps++, *m_modules, m_scope_manager.get());
    Cleanup();
}

auto spp::compiler::Compiler::Cleanup() -> void {
    asts::generate::common_types_precompiled::ClearTypes();
    analyse::scopes::ScopeManager::Cleanup();
}

SPP_MOD_END
