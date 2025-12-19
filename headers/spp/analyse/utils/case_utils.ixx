module;
#include <spp/macros.hpp>

export module spp.analyse.utils.case_utils;
import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class ScopeManager;
}

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantAst;
}

namespace spp::asts::meta {
    SPP_EXP_CLS struct CompilerMetaData;
}

namespace spp::codegen {
    SPP_EXP_CLS class LLvmCtx;
}


namespace spp::analyse::utils::case_utils {
    SPP_EXP_FUN auto create_and_analyse_pattern_eq_funcs(
        std::vector<asts::CasePatternVariantAst*> elems,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta,
        codegen::LLvmCtx *ctx)
        -> std::vector<llvm::Value*>;

    SPP_EXP_FUN auto create_and_analyse_pattern_eq_funcs_dummy(
        std::vector<asts::CasePatternVariantAst*> elems,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> void;
}
