#pragma once
#include <spp/asts/generic_argument_comp_ast.hpp>


/**
 * The GenericArgumentCompPositionalAst represents a positional argument in a generic argument context. It is forces the
 * argument to be matched by an index rather than a keyword.
 */
struct spp::asts::GenericArgumentCompPositionalAst final : GenericArgumentCompAst {
    SPP_AST_KEY_FUNCTIONS;

protected:
    auto equals(GenericArgumentAst const &other) const -> bool override;

    auto equals_generic_argument_comp_positional(GenericArgumentCompPositionalAst const &) const -> bool override;

public:
    /**
     * Construct the GenericArgumentCompPositionalAst with the arguments matching the members.
     * @param val The value of the generic comp argument.
     */
    explicit GenericArgumentCompPositionalAst(
        decltype(val) &&val);

    ~GenericArgumentCompPositionalAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
