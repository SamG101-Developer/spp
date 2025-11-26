module;
#include <spp/macros.hpp>

export module spp.compiler.compiler_boot;
import spp.analyse.scopes.scope_manager;
import spp.asts._fwd;
import spp.compiler.module_tree;

import indicators;
import std;



namespace spp::compiler {
    SPP_EXP struct CompilerBoot;
}


SPP_EXP struct spp::compiler::CompilerBoot {
private:
    std::vector<asts::ModulePrototypeAst*> m_modules;

    auto validate_entry_point(analyse::scopes::ScopeManager *sm) -> void;
    static auto move_scope_manager_to_ns(analyse::scopes::ScopeManager *sm, Module const&mod) -> void;

public:
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
};
