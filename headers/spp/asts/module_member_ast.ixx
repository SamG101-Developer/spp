module;
#include <spp/macros.hpp>

export module spp.asts.module_member_ast;
import spp.asts.ast;


SPP_EXP struct spp::asts::ModuleMemberAst : virtual Ast {
    ~ModuleMemberAst() override;
};
