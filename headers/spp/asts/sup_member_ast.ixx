module;
#include <spp/macros.hpp>

export module spp.asts.sup_member_ast;
import spp.asts.ast;

namespace spp::asts {
    SPP_EXP_CLS struct SupMemberAst;
}

/**
 * The SupMemberAst class is the base class for all members of a class in the abstract syntax tree. This is
 * applicable to the CmpStatementAst, FunctionPrototypeAst, TypeStatementAst publicly, but internally (for preprocessed
 * asts, it also includes the ClassPrototypeAst and SupPrototypeExtensionAst).
 * @note: This is a tag-based base class.
 */
SPP_EXP_CLS struct spp::asts::SupMemberAst {
    SupMemberAst();

    virtual ~SupMemberAst();
};
