module;
#include <spp/macros.hpp>

export module spp.compiler:compiler_boot;
import spp.abstract;
import spp.analyse.scopes;
import spp.codegen.llvm_ctx;
import spp.compiler.module_tree;
import spp.utils.progress;
import llvm;
import std;


namespace spp::compiler {
    SPP_EXP_CLS struct CompilerBoot;
}


SPP_EXP_CLS struct spp::compiler::CompilerBoot {
private:
    std::vector<AbstractAst*> m_modules;
    std::vector<std::unique_ptr<codegen::LlvmCtx>> m_llvm_ctxs;

    auto validate_entry_point(analyse::scopes::ScopeManager *sm) -> void;
    static auto move_scope_manager_to_ns(analyse::scopes::ScopeManager *sm, Module const &mod) -> void;

public:
    auto lex(utils::ProgressBar &bar, ModuleTree &tree) -> void;
    auto parse(utils::ProgressBar &bar, ModuleTree &tree) -> void;
    auto stage_1_pre_process(utils::ProgressBar &bar, ModuleTree &tree, AbstractAst *ctx) -> void;
    auto stage_2_gen_top_level_scopes(utils::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto stage_3_gen_top_level_aliases(utils::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto stage_4_qualify_types(utils::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto stage_5_load_super_scopes(utils::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto stage_6_pre_analyse_semantics(utils::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto stage_7_analyse_semantics(utils::ProgressBar &bar, ModuleTree &tree, bool is_exe, analyse::scopes::ScopeManager *sm) -> void;
    auto stage_8_check_memory(utils::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto stage_9_comptime_resolution(utils::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto stage_10_code_gen_1(utils::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto stage_11_code_gen_2(utils::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
};
