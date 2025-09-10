#pragma once
#include <spp/asts/ast.hpp>


namespace spp::asts::detail {
    template <typename GenericParameterType>
    struct make_required_param {
        using type = GenericParameterType;
    };

    template <>
    struct make_required_param<GenericParameterCompAst> {
        using type = GenericParameterCompAst;
    };

    template <>
    struct make_required_param<GenericParameterTypeAst> {
        using type = GenericParameterTypeAst;
    };

    template <typename GenericParameterType>
    using make_required_param_t = typename make_required_param<GenericParameterType>::type;

    template <typename GenericParameterType>
    struct make_optional_param {
        using type = GenericParameterType;
    };

    template <>
    struct make_optional_param<GenericParameterCompAst> {
        using type = GenericParameterCompOptionalAst;
    };

    template <>
    struct make_optional_param<GenericParameterTypeAst> {
        using type = GenericParameterTypeOptionalAst;
    };

    template <typename GenericParameterType>
    using make_optional_param_t = typename make_optional_param<GenericParameterType>::type;


    template <typename GenericParameterType>
    struct make_variadic_param {
        using type = GenericParameterType;
    };

    template <>
    struct make_variadic_param<GenericParameterCompAst> {
        using type = GenericParameterCompVariadicAst;
    };

    template <>
    struct make_variadic_param<GenericParameterTypeAst> {
        using type = GenericParameterTypeVariadicAst;
    };

    template <typename GenericParameterType>
    using make_variadic_param_t = typename make_variadic_param<GenericParameterType>::type;
}


/**
 * The GenericParameterAst is the base class for all generic parameters. It is inherited by the GenericParameterCompAst
 * and GenericParameterTypeAst, which represent the two types of generic parameters in the language.
 */
struct spp::asts::GenericParameterAst : virtual Ast {
    using Ast::Ast;

    /**
     * The name of the generic type parameter. This is the name that will be used to refer to the type parameter in the
     * generic type.
     */
    std::shared_ptr<TypeAst> name;

    explicit GenericParameterAst(std::shared_ptr<TypeAst> name);
};
