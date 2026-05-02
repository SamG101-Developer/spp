module;
#include <spp/macros.hpp>

export module spp.asts.class_implementation_ast;
import spp.asts.inner_scope_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct ClassImplementationAst;
}


SPP_EXP_CLS struct spp::asts::ClassImplementationAst final : InnerScopeAst<Unique<Ast>> {
    static auto NewEmpty() -> Unique<ClassImplementationAst>;

    using InnerScopeAst::InnerScopeAst;

    ~ClassImplementationAst() override;

    SPP_ATTR_NODISCARD auto Clone() const -> Unique<Ast> override;

    auto Stage1_PreProcess(Ast *ctx) -> void override;

    auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *) -> void override;

    auto Stage3_GenTopLvlAliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage5_LoadSupScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage6_PreAnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
