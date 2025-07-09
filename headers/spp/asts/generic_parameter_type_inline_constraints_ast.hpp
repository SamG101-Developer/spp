#ifndef GENERIC_PARAMETER_TYPE_INLINE_CONSTRAINTS_AST_HPP
#define GENERIC_PARAMETER_TYPE_INLINE_CONSTRAINTS_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::GenericParameterTypeInlineConstraintsAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @code :@endcode token that introduces the inline constraints.
     */
    std::unique_ptr<TokenAst> tok_colon;

    /**
     * The constraints for the generic type parameter. Any generic argument passed into the generic parameter must
     * satisfy these constraints.
     */
    std::vector<std::unique_ptr<TypeAst>> constraints;

    /**
     * Construct the GenericParameterTypeInlineConstraintsAst with the arguments matching the members.
     * @param tok_colon The @c : token that introduces the inline constraints.
     * @param constraints The constraints for the generic type parameter.
     */
    GenericParameterTypeInlineConstraintsAst(
        decltype(tok_colon) &&tok_colon,
        decltype(constraints) &&constraints);
};


#endif //GENERIC_PARAMETER_TYPE_INLINE_CONSTRAINTS_AST_HPP
