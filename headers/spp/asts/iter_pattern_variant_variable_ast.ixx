module;
#include <spp/macros.hpp>

export module spp.asts.iter_pattern_variant_variable_ast;
import spp.asts.iter_pattern_variant_ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IterPatternVariantVariableAst;
    SPP_EXP_CLS struct LocalVariableAst;
}


SPP_EXP_CLS struct spp::asts::IterPatternVariantVariableAst final : IterPatternVariantAst {
    /**
     * The variable that the generator's yielded value is bound to. This allows the internal value inside the
     * @c Generated container to be inspected.
     */
    std::unique_ptr<LocalVariableAst> var;

    /**
     * Constructor for the @c IterPatternVariantVariableAst.
     * @param var The variable that the generator's yielded value is bound to.
     */
    explicit IterPatternVariantVariableAst(
        decltype(var) &&var);

    ~IterPatternVariantVariableAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
