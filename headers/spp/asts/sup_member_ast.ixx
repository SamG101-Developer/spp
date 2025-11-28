module;
#include <spp/macros.hpp>

export module spp.asts.sup_member_ast;
import spp.asts._fwd;
import spp.asts.ast;

namespace spp::asts {
    SPP_EXP_CLS struct SupMemberAst;
}


SPP_EXP_CLS struct spp::asts::SupMemberAst : virtual Ast {
    ~SupMemberAst() override;
};
