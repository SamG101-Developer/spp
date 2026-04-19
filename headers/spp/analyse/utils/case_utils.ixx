module;
#include <spp/macros.hpp>

export module spp.analyse.utils.case_utils;
import spp.analyse.scopes;
import spp.asts;
import spp.codegen.llvm_ctx;
import llvm;
import std;


namespace spp::analyse::utils::case_utils {
    SPP_EXP_FUN template <typename T>
    auto create_and_analyse_pattern_eq_funcs_core(
        std::vector<asts::CasePatternVariantAst*> const &elems,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta,
        std::function<T(asts::Ast*)> &&mapper)
        -> std::vector<T>;

    SPP_EXP_FUN auto create_and_analyse_pattern_eq_funcs_llvm(
        std::vector<asts::CasePatternVariantAst*> const &elems,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta,
        codegen::LlvmCtx *ctx)
        -> std::vector<llvm::Value*>;

    SPP_EXP_FUN auto create_and_analyse_pattern_eq_comptime(
        std::vector<asts::CasePatternVariantAst*> const &elems,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> std::vector<std::unique_ptr<asts::ExpressionAst>>;

    SPP_EXP_FUN auto create_and_analyse_pattern_eq_funcs_dummy_core(
        std::vector<asts::CasePatternVariantAst*> const &elems,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> void;
}
