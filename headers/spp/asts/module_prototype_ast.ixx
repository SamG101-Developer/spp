module;
#include <spp/macros.hpp>

export module spp.asts.module_prototype_ast;
import spp.asts.ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::compiler {
    SPP_EXP_CLS struct CompilerBoot;
}

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct FunctionPrototypeAst;
    SPP_EXP_CLS struct ModulePrototypeAst;
    SPP_EXP_CLS struct ModuleImplementationAst;
}


/**
 * The ModulePrototypeAst represents a prototype for a module in the SPP language. It contains a the implementation of
 * the module.
 */
SPP_EXP_CLS struct spp::asts::ModulePrototypeAst final : Ast {
    /**
     * The file path of the module prototype. This is interacted with by the compiler to resolve module imports. Not got
     * from parsing children AST nodes.
     */
    std::filesystem::path FilePath = "";

    /**
     * The module implementation AST that this prototype represents.
     */
    Unique<ModuleImplementationAst> Impl;

    /**
     * Construct the ModulePrototypeAst with the given implementation.
     * @param[in] impl The module implementation AST that this prototype represents.
     */
    explicit ModulePrototypeAst(
        decltype(Impl) &&impl);

    ~ModulePrototypeAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage1_PreProcess(Ast *ctx) -> void override;

    auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *) -> void override;

    auto Stage3_GenTopLvlAliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage5_LoadSupScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage6_PreAnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage10_PreCodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    SPP_ATTR_NODISCARD auto Name() const -> Unique<IdentifierAst>;

    SPP_ATTR_NODISCARD auto FileName() const -> Unique<IdentifierAst>;
};
