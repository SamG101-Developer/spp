module;
#include <spp/macros.hpp>

export module spp.analyse.utils.destructure_utils;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct LocalVariableAst;
}


namespace spp::analyse::utils::destructure_utils {
    constexpr auto kUnmatchableTag = "_UNMATCHABLE";

    SPP_EXP_FUN auto GetNestedBindingIdentifiers(
        Vec<Unique<asts::LocalVariableAst>> const &elems)
        -> Vec<Shared<asts::IdentifierAst>>;

    SPP_EXP_FUN auto UnmatchableSingleIdentifier(
        std::size_t pos)
        -> Shared<asts::IdentifierAst>;
}
