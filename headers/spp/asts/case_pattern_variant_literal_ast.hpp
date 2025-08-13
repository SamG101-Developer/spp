#ifndef CASE_PATTERN_VARIANT_LITERAL_AST_HPP
#define CASE_PATTERN_VARIANT_LITERAL_AST_HPP

#include <spp/asts/case_pattern_variant_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::CasePatternVariantLiteralAst final : CasePatternVariantAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The literal value of the case pattern variant. This can be a string, integer, float, boolean, but not a tuple or
     * array; special destructure syntax exists for those literals.
     */
    std::unique_ptr<LiteralAst> literal;

    /**
     * Construct the CasePatternVariantLiteralAst with the arguments matching the members.
     * @param literal The literal value of the case pattern variant.
     */
    explicit CasePatternVariantLiteralAst(
        decltype(literal) &&literal);

    ~CasePatternVariantLiteralAst() override;

    auto convert_to_variable(mixins::CompilerMetaData *meta) -> std::unique_ptr<LocalVariableAst> override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};


#endif //CASE_PATTERN_VARIANT_LITERAL_AST_HPP
