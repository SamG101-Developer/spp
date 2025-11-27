module;
#include <spp/macros.hpp>

export module spp.asts.sup_member_ast;
import spp.asts.ast;


SPP_EXP_CLS struct spp::asts::SupMemberAst : virtual Ast {
    ~SupMemberAst() override;
};
