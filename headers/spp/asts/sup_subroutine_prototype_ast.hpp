#pragma once
#include <spp/asts/_fwd.hpp>
#include <spp/asts/subroutine_prototype_ast.hpp>
#include <spp/asts/sup_member_ast.hpp>


struct spp::asts::SupSubroutinePrototypeAst final : virtual SupMemberAst, virtual SubroutinePrototypeAst {
    using SubroutinePrototypeAst::SubroutinePrototypeAst;
};
