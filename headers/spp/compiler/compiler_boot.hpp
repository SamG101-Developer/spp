#pragma once
#include <spp/compiler/module_tree.hpp>
#include <indicators/progress_bar.hpp>

/// @cond
namespace spp::compiler {
    struct CompilerBoot;
}

namespace spp::analyse::scopes {
    class ScopeManager;
}
/// @endcond


struct spp::compiler::CompilerBoot {
    auto lex(indicators::ProgressBar &bar, ModuleTree &tree) -> void;
    auto parse(indicators::ProgressBar &bar, ModuleTree &tree) -> void;
    auto stage_1_pre_process(indicators::ProgressBar &bar, ModuleTree &tree, asts::Ast *ctx) -> void;
    auto stage_2_gen_top_level_scopes(indicators::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto stage_3_gen_top_level_aliases(indicators::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto stage_4_qualify_types(indicators::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto stage_5_load_super_scopes(indicators::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto stage_6_pre_analyse_semantics(indicators::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto stage_7_analyse_semantics(indicators::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto stage_8_check_memory(indicators::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;

private:
    auto validate_entry_point(analyse::scopes::ScopeManager *sm) -> void;
    static auto move_scope_manager_to_ns(analyse::scopes::ScopeManager *sm, Module const&mod) -> void;

    std::vector<asts::ModulePrototypeAst*> m_modules;
};
