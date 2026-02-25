module;
#include <spp/macros.hpp>

export module spp.asts.function_prototype_ast;
import spp.asts.ast;
import spp.asts.module_member_ast;
import spp.asts.sup_member_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
    SPP_EXP_CLS struct FunctionImplementationAst;
    SPP_EXP_CLS struct FunctionParameterGroupAst;
    SPP_EXP_CLS struct FunctionPrototypeAst;
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
}


/**
 * The @c FunctionPrototypeAst represents the prototype of a function. It defines the structure of a function, including
 * its name, parameters, and return type. The body of the function is defined in the FunctionImplementationAst.
 *
 * @n
 * This ASt is further inherited into the SubroutinePrototypeAst and CoroutinePrototypeAst, which add additional
 * analysis checks.
 */
SPP_EXP_CLS struct spp::asts::FunctionPrototypeAst : Ast, SupMemberAst, ModuleMemberAst {
    std::vector<std::unique_ptr<AnnotationAst>> annotations;
    std::unique_ptr<TokenAst> tok_cmp;
    std::unique_ptr<IdentifierAst> name;
    std::unique_ptr<GenericParameterGroupAst> generic_param_group;
    std::unique_ptr<FunctionParameterGroupAst> param_group;
    std::unique_ptr<TypeAst> return_type;
    std::unique_ptr<FunctionImplementationAst> impl;

    FunctionPrototypeAst(
        decltype(annotations) &&annotations,
        decltype(name) &&name,
        decltype(generic_param_group) &&generic_param_group,
        decltype(param_group) &&param_group,
        decltype(return_type) &&return_type,
        decltype(impl) &&impl);
    ~FunctionPrototypeAst() override;
    auto to_rust() const -> std::string override;
};
