module;
#include <spp/macros.hpp>

export module spp.asts:generic_parameter_ast;
import :ast;
import :orderable_ast;
import spp.asts.utils;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct TypeAst;
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
