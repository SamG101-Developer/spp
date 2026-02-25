module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The GenericParameterAst is the base class for all generic parameters. It is inherited by the GenericParameterCompAst
 * and GenericParameterTypeAst, which represent the two types of generic parameters in the language.
 */
SPP_EXP_CLS struct spp::asts::GenericParameterAst : Ast {
    explicit GenericParameterAst();
    ~GenericParameterAst() override;
};
