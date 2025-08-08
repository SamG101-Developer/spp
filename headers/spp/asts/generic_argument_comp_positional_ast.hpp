#ifndef GENERIC_ARGUMENT_COMP_POSITIONAL_AST_HPP
#define GENERIC_ARGUMENT_COMP_POSITIONAL_AST_HPP

#include <spp/asts/generic_argument_comp_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The GenericArgumentCompPositionalAst represents a positional argument in a generic argument context. It is forces the
 * argument to be matched by an index rather than a keyword.
 */
struct spp::asts::GenericArgumentCompPositionalAst final : GenericArgumentCompAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * Construct the GenericArgumentCompPositionalAst with the arguments matching the members.
     * @param val The value of the generic comp argument.
     */
    explicit GenericArgumentCompPositionalAst(
        decltype(val) &&val);

    ~GenericArgumentCompPositionalAst() override;
};


#endif //GENERIC_ARGUMENT_COMP_POSITIONAL_AST_HPP
