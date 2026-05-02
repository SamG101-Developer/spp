module;
#include <spp/macros.hpp>

export module spp.asts.module_member_ast;
import spp.asts.ast;

namespace spp::asts {
    SPP_EXP_CLS struct ModuleMemberAst;
}

/**
 * The ModuleMemberAst class is the base class for all members of a module in the abstract syntax tree. This is
 * applicable to CmpStatementAst, ClassPrototypeAst, FunctionPrototypeAst, SupPrototypeExtensionAst,
 * SupPrototypeFunctionsAst, TypeStatementAst, UseStatementAst, UseVariableStatementAst.
 * @note: This is a tag-based base class.
 */
SPP_EXP_CLS struct spp::asts::ModuleMemberAst {
    ModuleMemberAst();

    virtual ~ModuleMemberAst();
};
