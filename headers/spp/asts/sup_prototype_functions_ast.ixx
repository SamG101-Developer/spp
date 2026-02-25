module;
#include <spp/macros.hpp>

export module spp.asts.sup_prototype_functions_ast;
import spp.asts.ast;
import spp.asts.module_member_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct SupImplementationAst;
    SPP_EXP_CLS struct SupPrototypeFunctionsAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The SupPrototypeFunctionsAst represents a superimposition of methods over a type. This is used to add behavior to a
 * type. For example, to extend the @c std::Str type with additional methods, the following code can be used:
 * @code
 * sup std::Str {
 *     fun to_upper() -> std::Str { ... }
 * }
 * @endcode
 */
SPP_EXP_CLS struct spp::asts::SupPrototypeFunctionsAst final : Ast, ModuleMemberAst {
    std::unique_ptr<GenericParameterGroupAst> generic_param_group;
    std::unique_ptr<TypeAst> name;
    std::unique_ptr<SupImplementationAst> impl;

    SupPrototypeFunctionsAst(
        decltype(generic_param_group) &&generic_param_group,
        decltype(name) &&name,
        decltype(impl) &&impl);
    ~SupPrototypeFunctionsAst() override;
    auto to_rust() const -> std::string override;
};
