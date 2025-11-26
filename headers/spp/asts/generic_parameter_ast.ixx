module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_ast;
import spp.asts._fwd;
import spp.asts.mixins.orderable_ast;
import spp.asts.ast;

import std;


namespace spp::asts::detail {
    SPP_EXP template <typename GenericParameterType>
    struct make_required_param {
        using type = GenericParameterType;
    };

    SPP_EXP template <>
    struct make_required_param<GenericParameterCompAst> {
        using type = GenericParameterCompAst;
    };

    SPP_EXP template <>
    struct make_required_param<GenericParameterTypeAst> {
        using type = GenericParameterTypeAst;
    };

    SPP_EXP template <typename GenericParameterType>
    using make_required_param_t = typename make_required_param<GenericParameterType>::type;

    SPP_EXP template <typename GenericParameterType>
    struct make_optional_param {
        using type = GenericParameterType;
    };

    SPP_EXP template <>
    struct make_optional_param<GenericParameterCompAst> {
        using type = GenericParameterCompOptionalAst;
    };

    SPP_EXP template <>
    struct make_optional_param<GenericParameterTypeAst> {
        using type = GenericParameterTypeOptionalAst;
    };

    SPP_EXP template <typename GenericParameterType>
    using make_optional_param_t = typename make_optional_param<GenericParameterType>::type;


    SPP_EXP template <typename GenericParameterType>
    struct make_variadic_param {
        using type = GenericParameterType;
    };

    SPP_EXP template <>
    struct make_variadic_param<GenericParameterCompAst> {
        using type = GenericParameterCompVariadicAst;
    };

    SPP_EXP template <>
    struct make_variadic_param<GenericParameterTypeAst> {
        using type = GenericParameterTypeVariadicAst;
    };

    SPP_EXP template <typename GenericParameterType>
    using make_variadic_param_t = typename make_variadic_param<GenericParameterType>::type;


    SPP_EXP template <typename GenericParameterType>
    struct generic_param_value_type;

    SPP_EXP template <>
    struct generic_param_value_type<GenericParameterCompAst> {
        using type = ExpressionAst const*;
    };

    SPP_EXP template <>
    struct generic_param_value_type<GenericParameterTypeAst> {
        using type = std::shared_ptr<TypeAst>;
    };

    SPP_EXP template <typename GenericParameterType>
    using value_type_t = typename generic_param_value_type<GenericParameterType>::type;
}


/**
 * The GenericParameterAst is the base class for all generic parameters. It is inherited by the GenericParameterCompAst
 * and GenericParameterTypeAst, which represent the two types of generic parameters in the language.
 */
SPP_EXP struct spp::asts::GenericParameterAst : virtual Ast, mixins::OrderableAst {
    using Ast::Ast;

    /**
     * The name of the generic type parameter. This is the name that will be used to refer to the type parameter in the
     * generic type.
     */
    std::shared_ptr<TypeAst> name;

    explicit GenericParameterAst(
        std::shared_ptr<TypeAst> name,
        decltype(m_order_tag) order_tag);
};
