#ifndef GENERIC_PARAMETER_TYPE_AST_HPP
#define GENERIC_PARAMETER_TYPE_AST_HPP

#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::GenericParameterTypeAst : GenericParameterAst {
    /**
     * The name of the generic type parameter. This is the name that will be used to refer to the type parameter in the
     * generic type.
     */
    std::unique_ptr<TypeAst> name;

    /**
     * The optional inline constraints for the generic type parameter. This is used to specify constraints on the type
     * parameter, such as @c I32 or @c F64 . An example is @code fun func[T: Copy]()@endcode, where @c T is the
     * generic type parameter and @c Copy is the constraint.
     */
    std::unique_ptr<GenericParameterTypeInlineConstraintsAst> constraints;

    /**
     * Construct the GenericParameterTypeAst with the arguments matching the members.
     * @param name The name of the generic type parameter.
     * @param constraints The optional inline constraints for the generic type parameter.
     */
    GenericParameterTypeAst(
        decltype(name) &&name,
        decltype(constraints) &&constraints);
};


#endif //GENERIC_PARAMETER_TYPE_AST_HPP
