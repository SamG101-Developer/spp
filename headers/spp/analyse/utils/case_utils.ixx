module;
#include <spp/macros.hpp>

export module spp.analyse.utils.case_utils;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class ScopeManager;
}

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct CasePatternVariantAst;
    SPP_EXP_CLS struct ExpressionAst;
}

namespace spp::asts::meta {
    SPP_EXP_CLS struct CompilerMetaData;
}

namespace spp::codegen {
    SPP_EXP_CLS class LLvmCtx;
}


namespace spp::analyse::utils::case_utils {
    SPP_EXP_FUN template <typename T>
    auto CreateAndAnalysePatternEqFuncsCore(
        Vec<asts::CasePatternVariantAst*> const &elems,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta,
        std::copyable_function<T(asts::Ast *)> &&mapper) // todo: function_ref?
        -> Vec<T>;

    SPP_EXP_FUN auto CreateAndAnalysePatternEqFuncsLlvm(
        Vec<asts::CasePatternVariantAst*> const &elems,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta,
        codegen::LLvmCtx *ctx)
        -> Vec<llvm::Value*>;

    SPP_EXP_FUN auto CreateAndAnalysePatternEqCompTime(
        Vec<asts::CasePatternVariantAst*> const &elems,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> Vec<Unique<asts::ExpressionAst>>;

    SPP_EXP_FUN auto CreateAndAnalysePatternEqFuncsDummyCore(
        Vec<asts::CasePatternVariantAst*> const &elems,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> void;
}
