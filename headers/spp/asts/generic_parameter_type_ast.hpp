#pragma once
#include <spp/asts/generic_parameter_ast.hpp>


struct spp::asts::GenericParameterTypeAst : GenericParameterAst {

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
        decltype(name) name,
        decltype(constraints) &&constraints);

    ~GenericParameterTypeAst() override;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, mixins::CompilerMetaData *) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
