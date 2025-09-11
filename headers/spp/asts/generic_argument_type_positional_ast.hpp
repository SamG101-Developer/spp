#pragma once
#include <spp/asts/generic_argument_type_ast.hpp>


/**
 * The GenericArgumentTypePositionalAst represents a positional argument in a generic argument context. It is forces the
 * argument to be matched by an index rather than a keyword.
 */
struct spp::asts::GenericArgumentTypePositionalAst final : GenericArgumentTypeAst {
    SPP_AST_KEY_FUNCTIONS;

protected:
    auto equals(GenericArgumentAst const &other) const -> std::strong_ordering override;

    auto equals_generic_argument_type_positional(GenericArgumentTypePositionalAst const &) const -> std::strong_ordering override;

public:
    /**
     * Construct the GenericArgumentTypePositionalAst with the arguments matching the members.
     * @param val The value of the generic type argument.
     */
    explicit GenericArgumentTypePositionalAst(
        decltype(val) val);

    ~GenericArgumentTypePositionalAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
