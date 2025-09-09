#include <spp/compiler/compiler.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/module_prototype_ast.hpp>
#include <spp/asts/generate/common_types_precompiled.hpp>

#include <indicators/progress_bar.hpp>


spp::compiler::Compiler::Compiler(const Mode mode) :
    m_modules(std::filesystem::current_path()),
    m_mode(mode) {
    m_path = std::filesystem::current_path() / "src";
    m_boot = CompilerBoot();
}


auto spp::compiler::Compiler::compile() -> void {
    auto progress_bars = std::vector<std::unique_ptr<indicators::ProgressBar>>();
    for (auto stage : COMPILER_STAGE_NAMES) {
        auto p = std::make_unique<indicators::ProgressBar>();
        p->set_option(indicators::option::BarWidth{50});
        p->set_option(indicators::option::Start{"["});
        p->set_option(indicators::option::Fill{"="});
        p->set_option(indicators::option::Lead{">"});
        p->set_option(indicators::option::End{"]"});
        p->set_option(indicators::option::PrefixText{stage});
        p->set_option(indicators::option::ShowElapsedTime{true});
        p->set_option(indicators::option::ShowRemainingTime{true});
        p->set_option(indicators::option::MaxProgress{static_cast<int>(m_modules.get_modules().size())});
        progress_bars.emplace_back(std::move(p));
    }

    // Save the modules into the CompilerBoot instance, and lex/parse.
    auto ps = progress_bars.begin();
    m_boot.lex(**ps++, m_modules);
    m_boot.parse(**ps++, m_modules);
    m_scope_manager = std::make_unique<analyse::scopes::ScopeManager>(
        analyse::scopes::Scope::new_global(m_modules.get_modules()[0]), nullptr);
    asts::generate::common_types_precompiled::initialize_types();

    m_boot.stage_1_pre_process(**ps++, m_modules, nullptr);
    m_boot.stage_2_gen_top_level_scopes(**ps++, m_modules, m_scope_manager.get());
    m_boot.stage_3_gen_top_level_aliases(**ps++, m_modules, m_scope_manager.get());
    m_boot.stage_4_qualify_types(**ps++, m_modules, m_scope_manager.get());
    m_boot.stage_5_load_super_scopes(**ps++, m_modules, m_scope_manager.get());
    m_boot.stage_6_pre_analyse_semantics(**ps++, m_modules, m_scope_manager.get());
    m_boot.stage_7_analyse_semantics(**ps++, m_modules, m_scope_manager.get());
    m_boot.stage_8_check_memory(**ps++, m_modules, m_scope_manager.get());
}
