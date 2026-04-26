module;
#include <spp/macros.hpp>

export module spp.analyse.utils.destructure_utils;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct LocalVariableAst;
}


namespace spp::analyse::utils::destructure_utils {
    constexpr auto UNMATCHABLE_TAG = "_UNMATCHABLE";

    SPP_EXP_FUN auto get_nested_binding_identifiers(
        std::vector<std::unique_ptr<asts::LocalVariableAst>> const &elems)
        -> std::vector<std::shared_ptr<asts::IdentifierAst>>;

    SPP_EXP_FUN auto unmatchable_single_identifier(
        std::size_t pos)
        -> std::shared_ptr<asts::IdentifierAst>;
}
