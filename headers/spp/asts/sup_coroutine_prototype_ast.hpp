#pragma once
#include <spp/asts/_fwd.hpp>
#include <spp/asts/coroutine_prototype_ast.hpp>
#include <spp/asts/sup_member_ast.hpp>


struct spp::asts::SupCoroutinePrototypeAst final : virtual SupMemberAst, virtual CoroutinePrototypeAst {
    using CoroutinePrototypeAst::CoroutinePrototypeAst;
};
