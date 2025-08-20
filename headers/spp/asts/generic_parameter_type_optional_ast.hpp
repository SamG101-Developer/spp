#ifndef GENERIC_PARAMETER_TYPE_OPTIONAL_AST_HPP
#define GENERIC_PARAMETER_TYPE_OPTIONAL_AST_HPP

#include <spp/asts/generic_parameter_type_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::GenericParameterTypeOptionalAst final : GenericParameterTypeAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The token that separates the parameter name from the default value.
     */
    std::unique_ptr<TokenAst> tok_assign;

    /**
     * The default value for the parameter. This is the expression that will be used if the parameter is not provided.
     */
    std::shared_ptr<TypeAst> default_val;

    /**
     * Construct the GenericParameterTypeOptionalAst with the arguments matching the members.
     * @param name The name of the generic type parameter.
     * @param constraints The optional inline constraints for the generic type parameter.
     * @param tok_assign The token that separates the parameter name from the default value.
     * @param default_val The default value for the parameter.
     */
    GenericParameterTypeOptionalAst(
        decltype(name) &&name,
        decltype(constraints) &&constraints,
        decltype(tok_assign) &&tok_assign,
        decltype(default_val) &&default_val);

    ~GenericParameterTypeOptionalAst() override;

    auto stage_4_qualify_types(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};


#endif //GENERIC_PARAMETER_TYPE_OPTIONAL_AST_HPP
