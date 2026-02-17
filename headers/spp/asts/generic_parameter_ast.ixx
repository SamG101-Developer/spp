module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.orderable;
import spp.asts.ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct GenericParameterCompAst;
    SPP_EXP_CLS struct GenericParameterCompOptionalAst;
    SPP_EXP_CLS struct GenericParameterCompVariadicAst;
    SPP_EXP_CLS struct GenericParameterTypeAst;
    SPP_EXP_CLS struct GenericParameterTypeOptionalAst;
    SPP_EXP_CLS struct GenericParameterTypeVariadicAst;
    SPP_EXP_CLS struct TypeAst;
}


namespace spp::asts::detail {
    SPP_EXP_CLS template <typename GenericParameterType>
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

    SPP_EXP_CLS template <typename GenericParameterType>
    using make_required_param_t = typename make_required_param<GenericParameterType>::type;


    SPP_EXP_CLS template <typename GenericParameterType>
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

    SPP_EXP_CLS template <typename GenericParameterType>
    using make_optional_param_t = typename make_optional_param<GenericParameterType>::type;


    SPP_EXP_CLS template <typename GenericParameterType>
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

    SPP_EXP_CLS template <typename GenericParameterType>
    using make_variadic_param_t = typename make_variadic_param<GenericParameterType>::type;


    SPP_EXP_CLS template <typename GenericParameterType>
    struct generic_param_value_type;

    template <>
    struct generic_param_value_type<GenericParameterCompAst> {
        using type = ExpressionAst const*;
    };

    template <>
    struct generic_param_value_type<GenericParameterTypeAst> {
        using type = std::shared_ptr<TypeAst>;
    };

    SPP_EXP_CLS template <typename GenericParameterType>
    using value_type_t = typename generic_param_value_type<GenericParameterType>::type;
}


/**
 * The GenericParameterAst is the base class for all generic parameters. It is inherited by the GenericParameterCompAst
 * and GenericParameterTypeAst, which represent the two types of generic parameters in the language.
 */
SPP_EXP_CLS struct spp::asts::GenericParameterAst : virtual Ast, mixins::OrderableAst {
    /**
     * The name of the generic type parameter. This is the name that will be used to refer to the type parameter in the
     * generic type.
     */
    std::shared_ptr<TypeAst> name;

    explicit GenericParameterAst(
        std::shared_ptr<TypeAst> name,
        utils::OrderableTag order_tag);

    ~GenericParameterAst() override;
};
