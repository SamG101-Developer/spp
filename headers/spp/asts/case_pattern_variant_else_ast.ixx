module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_else_ast;
import spp.asts._fwd;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;

import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantElseAst;
}


SPP_EXP_CLS struct spp::asts::CasePatternVariantElseAst final : CasePatternVariantAst {
    /**
     * The @c else keyword that indicates this is an else branch of the case pattern variant.
     */
    std::unique_ptr<TokenAst> tok_else;

    /**
     * Construct the CasePatternVariantElseAst with the arguments matching the members.
     * @param tok_else The @c else keyword that indicates this is an else branch of the case pattern variant.
     */
    explicit CasePatternVariantElseAst(
        decltype(tok_else) &&tok_else);

    ~CasePatternVariantElseAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
