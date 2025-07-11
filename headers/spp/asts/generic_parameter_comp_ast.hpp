#ifndef GENERIC_PARAMETER_COMP_AST_HPP
#define GENERIC_PARAMETER_COMP_AST_HPP

#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::GenericParameterCompAst : GenericParameterAst {
    /**
     * The @c cmp token that represents the generic comp parameter. This is used to indicate that the parameter is a
     * comp generic and not a type generic.
     */
    std::unique_ptr<TokenAst> tok_cmp;

    /**
     * The value of the generic comp parameter. This is passed into the generic like
     * @code fun func[cmp x: BigInt]()@endcode or @code std::Arr[std::String, 100_uz]@endcode.
     */
    std::unique_ptr<TypeAst> name;

    /**
     * The token that represents the @c : colon in the generic parameter. This separates the parameter name from the
     * type.
     */
    std::unique_ptr<TokenAst> tok_colon;

    /**
     * The type of the parameter. This is used to specify the type of the generic comp parameter, such as @c I32 or
     * @c F64 . This is a required field, as the type of the parameter must be known at compile time.
     */
    std::unique_ptr<TypeAst> type;

    /**
     * Construct the GenericParameterCompAst with the arguments matching the members.
     * @param tok_cmp The @c cmp token that represents the generic comp parameter.
     * @param name The value of the generic comp parameter.
     * @param tok_colon The token that represents the @c : colon in the generic parameter.
     * @param type The type of the parameter.
     */
    GenericParameterCompAst(
        decltype(tok_cmp) &&tok_cmp,
        decltype(name) &&name,
        decltype(tok_colon) &&tok_colon,
        decltype(type) &&type);
};


#endif //GENERIC_PARAMETER_COMP_AST_HPP
