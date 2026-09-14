module;
#include <spp/macros.hpp>

export module spp.asts.sup_member_ast;
import spp.asts.ast;

SPP_AST_COMMON_FWD_DECL(SupMemberAst);

/// The tag-based base class for all members of a class. This
/// is applicable to the CmpStatementAst, FunctionPrototypeAst
/// and TypeStatementAst publicly, and internally (for
/// preprocessed asts) also the ClassPrototypeAst and
/// SupPrototypeExtensionAst.
SPP_EXP_CLS struct spp::asts::SupMemberAst {
  SupMemberAst();

  virtual ~SupMemberAst();
};
