module;
#include <spp/macros.hpp>

export module spp.asts.module_member_ast;
import spp.asts.ast;

namespace spp::asts {
    SPP_EXP_CLS struct ModuleMemberAst;
}


SPP_EXP_CLS struct spp::asts::ModuleMemberAst : virtual Ast {
    ModuleMemberAst() = default;

    ~ModuleMemberAst() override;
};
