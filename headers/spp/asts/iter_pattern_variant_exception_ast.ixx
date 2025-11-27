module;
#include <spp/macros.hpp>

export module spp.asts.iter_pattern_variant_exception_ast;
import spp.asts.iter_pattern_variant_ast;

import std;


SPP_EXP_CLS struct spp::asts::IterPatternVariantExceptionAst final : IterPatternVariantAst {
    /**
     * The @c ! token that indicates the pattern variant is an exception. This is used with @c GenRes generators.
     */
    std::unique_ptr<TokenAst> tok_exc;

    /**
     * The variable binding to the error value. This allows the error to be lifted into the caller, or its value to be
     * inspected and modified.
     */
    std::unique_ptr<LocalVariableAst> var;

    /**
     * Constructor for the @c IterPatternVariantExceptionAst.
     * @param tok_exc The @c ! token that indicates the pattern variant is an exception.
     * @param var The variable binding to the error value.
     */
    IterPatternVariantExceptionAst(
        decltype(tok_exc) &&tok_exc,
        decltype(var) &&var);

    ~IterPatternVariantExceptionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
