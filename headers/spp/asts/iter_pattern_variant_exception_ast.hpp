#ifndef ITER_PATTERN_VARIANT_EXCEPTION_AST_HPP
#define ITER_PATTERN_VARIANT_EXCEPTION_AST_HPP

#include <spp/asts/iter_pattern_variant_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::IterPatternVariantExceptionAst final : IterPatternVariantAst {
    SPP_AST_KEY_FUNCTIONS;

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
};


#endif //ITER_PATTERN_VARIANT_EXCEPTION_AST_HPP
