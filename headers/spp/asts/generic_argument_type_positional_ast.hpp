#ifndef GENERIC_ARGUMENT_TYPE_POSITIONAL_AST_HPP
#define GENERIC_ARGUMENT_TYPE_POSITIONAL_AST_HPP

#include <spp/asts/generic_argument_type_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The GenericArgumentTypePositionalAst represents a positional argument in a generic argument context. It is forces the
 * argument to be matched by an index rather than a keyword.
 */
struct spp::asts::GenericArgumentTypePositionalAst final : GenericArgumentTypeAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * Construct the GenericArgumentTypePositionalAst with the arguments matching the members.
     * @param val The value of the generic type argument.
     */
    explicit GenericArgumentTypePositionalAst(
        decltype(val) &&val);

    ~GenericArgumentTypePositionalAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};


#endif //GENERIC_ARGUMENT_TYPE_POSITIONAL_AST_HPP
