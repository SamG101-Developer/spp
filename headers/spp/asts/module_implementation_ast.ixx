module;
#include <spp/macros.hpp>

export module spp.asts.module_implementation_ast;
import spp.asts.ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ModuleImplementationAst;
}


/**
 * The ModuleImplementationAst represents the implementation of a module in the SPP language. It contains a list of
 * module members that define the functionality and structure of the module.
 */
SPP_EXP_CLS struct spp::asts::ModuleImplementationAst final : Ast {
    /**
     * The list of module members in the implementation. This can include function implementations, class implementations,
     * and other module-level constructs.
     */
    Vec<Unique<Ast>> Members;

    /**
     * Construct the ModuleImplementationAst with the arguments matching the members.
     * @param[in] members The list of module members in the implementation.
     */
    explicit ModuleImplementationAst(
        decltype(Members) &&members);

    ~ModuleImplementationAst() override;

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

    auto Stage11_CodeGen(ScopeManager *, CompilerMetaData *, codegen::LLvmCtx *) -> llvm::Value* override;
};
