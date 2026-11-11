module;
#include <spp/macros.hpp>

export module spp.asts.module_member_ast;
import spp.asts.ast;

use_common(ModuleMemberAst);

/// The tag-based base class for all members of a module:
/// CmpStatementAst, ClassPrototypeAst, FunctionPrototypeAst,
/// SupPrototypeExtensionAst, SupPrototypeFunctionsAst,
/// TypeStatementAst, UseStatementAst, UseVariableStatementAst.
SPP_EXP_CLS struct spp::asts::ModuleMemberAst {
  ModuleMemberAst();

  virtual ~ModuleMemberAst();
};
