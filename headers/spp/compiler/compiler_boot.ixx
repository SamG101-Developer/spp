module;
#include <spp/macros.hpp>

export module spp.compiler.compiler_boot;
import spp.codegen.llvm_ctx;
import spp.utils.progress;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class ScopeManager;
}

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct ModulePrototypeAst;
}

namespace spp::compiler {
    SPP_EXP_CLS struct CompilerBoot;
    SPP_EXP_CLS struct Module;
    SPP_EXP_CLS struct ModuleTree;
}

SPP_EXP_CLS struct spp::compiler::CompilerBoot {
    auto Lex(utils::ProgressBar &bar, ModuleTree &tree) -> void;
    auto Parse(utils::ProgressBar &bar, ModuleTree &tree) -> void;
    auto Stage1_PreProcess(utils::ProgressBar &bar, ModuleTree &tree, asts::Ast *ctx) -> void;
    auto Stage2_GenTopLvlScopes(utils::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto Stage3_GenTopLvlAliases(utils::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto Stage4_QualifyTypes(utils::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto Stage5_LoadSupScopes(utils::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto Stage6_PreAnalyseSemantics(utils::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto Stage7_AnalyseSemantics(utils::ProgressBar &bar, ModuleTree &tree, bool is_exe, analyse::scopes::ScopeManager *sm) -> void;
    auto Stage8_CheckMemory(utils::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto Stage9_CompTimeResolve(utils::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto Stage10_PreCodeGen(utils::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;
    auto Stage11_CodeGen(utils::ProgressBar &bar, ModuleTree &tree, analyse::scopes::ScopeManager *sm) -> void;

private:
    Vec<asts::ModulePrototypeAst*> _Modules;
    Vec<Unique<codegen::LLvmCtx>> _LlvmCtxs;

    auto _ValidateEntryPoint(analyse::scopes::ScopeManager *sm) -> void;
    static auto _MoveScopeManagerToNs(analyse::scopes::ScopeManager *sm, Module const &mod) -> void;
};
