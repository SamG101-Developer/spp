module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;

import llvm;
import std;


SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureObjectAst final : CasePatternVariantAst {
    /**
     * The type of the object being destructured. This is used to determine the type of the destructured elements (by
     * attribute type inference)
     */
    std::shared_ptr<TypeAst> type;

    /**
     * The @code (@endcode token that indicates the start of a object destructuring pattern.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The elements of the object destructuring pattern. This is a list of patterns that will be destructured from the
     * object. Each element can be a single identifier, a nested destructuring pattern, or a literal.
     */
    std::vector<std::unique_ptr<CasePatternVariantAst>> elems;

    /**
     * The @code )@endcode token that indicates the end of an object destructuring pattern.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the CasePatternVariantDestructureObjectAst with the arguments matching the members.
     * @param[in] type The type of the object being destructured.
     * @param[in] tok_l The @code (@endcode token that indicates the start of a object destructuring pattern.
     * @param[in] elems The elements of the object destructuring pattern.
     * @param[in] tok_r The @code )@endcode token that indicates the end of a object destructuring pattern.
     */
    CasePatternVariantDestructureObjectAst(
        decltype(type) &&type,
        decltype(tok_l) &&tok_l,
        decltype(elems) &&elems,
        decltype(tok_r) &&tok_r);

    ~CasePatternVariantDestructureObjectAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto convert_to_variable(CompilerMetaData *meta) -> std::unique_ptr<LocalVariableAst> override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value * override;
};
