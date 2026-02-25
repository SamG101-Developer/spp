module;
#include <spp/macros.hpp>

export module spp.asts.sup_prototype_extension_ast;
import spp.asts.ast;
import spp.asts.module_member_ast;
import spp.asts.sup_member_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct SupImplementationAst;
    SPP_EXP_CLS struct SupPrototypeExtensionAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The SupPrototypeExtensionAst represents a superimposition of a type over a type. This is used to "inherit" a type.
 * For example, to extend the @c A type with @c B, the following code can be used:
 * @code
 * sup A ext B {
 *     # Override any abstract or virtual methods from B here.
 * }
 * @endcode
 */
SPP_EXP_CLS struct spp::asts::SupPrototypeExtensionAst final : Ast, ModuleMemberAst, SupMemberAst {
    std::unique_ptr<GenericParameterGroupAst> generic_param_group;
    std::unique_ptr<TypeAst> name;
    std::unique_ptr<TypeAst> super_class;
    std::unique_ptr<SupImplementationAst> impl;

    SupPrototypeExtensionAst(
        decltype(generic_param_group) &&generic_param_group,
        decltype(name) &&name,
        decltype(super_class) &&super_class,
        decltype(impl) &&impl);
    ~SupPrototypeExtensionAst() override;
    auto to_rust() const -> std::string override;
};
