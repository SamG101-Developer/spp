module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_array_ast;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantDestructureArrayAst;
    SPP_EXP_CLS struct LocalVariableAst;
    SPP_EXP_CLS struct TokenAst;
}

SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureArrayAst final : CasePatternVariantAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @code [@endcode token that indicates the start of an array destructuring pattern.
     */
    Unique<TokenAst> TokL;

    /**
     * The elements of the array destructuring pattern. This is a list of patterns that will be destructured from the
     * array. Each element can be a single identifier, a nested destructuring pattern, or a literal.
     */
    UniqueVec<CasePatternVariantAst> Elems;

    /**
     * The @code ]@endcode token that indicates the end of an array destructuring pattern.
     */
    Unique<TokenAst> TokR;

    /**
     * Construct the CasePatternVariantDestructureArrayAst with the arguments matching the members.
     * @param[in] tok_l The @code [@endcode token that indicates the start of an array destructuring pattern.
     * @param[in] elems The elements of the array destructuring pattern.
     * @param[in] tok_r The @code ]@endcode token that indicates the end of an array destructuring pattern.
     */
    CasePatternVariantDestructureArrayAst(decltype(TokL) &&tok_l, decltype(Elems) &&elems, decltype(TokR) &&tok_r);

    ~CasePatternVariantDestructureArrayAst() override;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value * override;

    auto ConvToVar(CompilerMetaData *meta) -> Unique<LocalVariableAst> override;
};
