module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_tuple_ast;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantDestructureTupleAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureTupleAst final : CasePatternVariantAst {
    /**
     * The @code (@endcode token that indicates the start of a tuple destructuring pattern.
     */
    Unique<TokenAst> TokL;

    /**
     * The elements of the tuple destructuring pattern. This is a list of patterns that will be destructured from the
     * tuple. Each element can be a single identifier, a nested destructuring pattern, or a literal.
     */
    UniqueVec<CasePatternVariantAst> Elems;

    /**
     * The @code )@endcode token that indicates the end of an tuple destructuring pattern.
     */
    Unique<TokenAst> TokR;

    /**
     * Construct the CasePatternVariantDestructureTupleAst with the arguments matching the members.
     * @param[in] tok_l The @code (@endcode token that indicates the start of a tuple destructuring pattern.
     * @param[in] elems The elements of the tuple destructuring pattern.
     * @param[in] tok_r The @code )@endcode token that indicates the end of a tuple destructuring pattern.
     */
    CasePatternVariantDestructureTupleAst(
        decltype(TokL) &&tok_l,
        decltype(Elems) &&elems,
        decltype(TokR) &&tok_r);

    ~CasePatternVariantDestructureTupleAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto ConvToVar(CompilerMetaData *meta) -> Unique<LocalVariableAst> override;
};
