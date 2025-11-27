module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_tuple_ast;
import spp.asts.case_pattern_variant_ast;

import llvm;
import std;


SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureTupleAst final : CasePatternVariantAst {
    /**
     * The @code (@endcode token that indicates the start of a tuple destructuring pattern.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The elements of the tuple destructuring pattern. This is a list of patterns that will be destructured from the
     * tuple. Each element can be a single identifier, a nested destructuring pattern, or a literal.
     */
    std::vector<std::unique_ptr<CasePatternVariantAst>> elems;

    /**
     * The @code )@endcode token that indicates the end of an tuple destructuring pattern.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the CasePatternVariantDestructureTupleAst with the arguments matching the members.
     * @param[in] tok_l The @code (@endcode token that indicates the start of a tuple destructuring pattern.
     * @param[in] elems The elements of the tuple destructuring pattern.
     * @param[in] tok_r The @code )@endcode token that indicates the end of a tuple destructuring pattern.
     */
    CasePatternVariantDestructureTupleAst(
        decltype(tok_l) &&tok_l,
        decltype(elems) &&elems,
        decltype(tok_r) &&tok_r);

    ~CasePatternVariantDestructureTupleAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto convert_to_variable(CompilerMetaData *meta) -> std::unique_ptr<LocalVariableAst> override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
