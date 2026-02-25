module;
#include <spp/macros.hpp>

export module spp.asts.type_identifier_ast;
import spp.asts.type_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
}


/**
 * The TypeIdentifierAst is a type expression that is represented by a single type name, and is analogous to the
 * IdentifierAst of the ExpressionAst.
 */
SPP_EXP_CLS struct spp::asts::TypeIdentifierAst final : TypeAst {
    std::string name;
    std::unique_ptr<GenericArgumentGroupAst> generic_arg_group;

    explicit TypeIdentifierAst(
        decltype(name) &&name,
        decltype(generic_arg_group) generic_arg_group);
    ~TypeIdentifierAst() override;
    auto to_rust() const -> std::string override;
};
