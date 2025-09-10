#pragma once
#include <spp/asts/generic_parameter_type_ast.hpp>


struct spp::asts::GenericParameterTypeRequiredAst final : GenericParameterTypeAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * Construct the GenericParameterTypeRequiredAst with the arguments matching the members.
     * @param name The name of the generic type parameter.
     * @param constraints The optional inline constraints for the generic type parameter.
     *
     * @note This constructor just calls the GenericParameterTypeAst constructor with the same arguments, but is defined
     * for uniformity with the other parameter variants.
     */
    GenericParameterTypeRequiredAst(
        decltype(name) &&name,
        decltype(constraints) &&constraints);

    ~GenericParameterTypeRequiredAst() override;
};
