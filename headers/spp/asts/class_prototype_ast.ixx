module;
#include <spp/macros.hpp>

export module spp.asts.class_prototype_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
    SPP_EXP_CLS struct ClassImplementationAst;
    SPP_EXP_CLS struct ClassPrototypeAst;
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeStatementAst;
}


/**
 * The ClassPrototypeAst represents the prototype of a class in the abstract syntax tree. It defines the structure of a
 * class, including its name and any generic parameters it may have. The attributes are defined in the implementation
 * ast for this class, allowing for scoping rules to be made easier.
 */
SPP_EXP_CLS struct spp::asts::ClassPrototypeAst final : Ast {
    std::vector<std::unique_ptr<AnnotationAst>> annotations;
    std::unique_ptr<TypeAst> name;
    std::unique_ptr<GenericParameterGroupAst> generic_param_group;
    std::unique_ptr<ClassImplementationAst> impl;

    ClassPrototypeAst(
        decltype(annotations) &&annotations,
        decltype(name) name,
        decltype(generic_param_group) &&generic_param_group,
        decltype(impl) &&impl);
    ~ClassPrototypeAst() override;
    auto to_rust() const -> std::string override;
};
