#pragma once

#include <spp/asts/_fwd.hpp>

namespace spp::asts::mixins {
    struct OrderableAst;
}

namespace spp::analyse::utils::order_utils {
    /**
     * Order the arguments in the AST by the ordering defined in the "ordering" parameter. This re-arranges the ASTs in
     * the seqeuence by sorting the values by their "m_order_tag" attribute, which is the one of the argument or
     * parameter ordering enumeration values. The original vector's order is preserved.
     * @param asts The ASTs to order. These must be of the @c OrderableAst type, or derived from it.
     * @return A new vector of ordered ASTs, preserving the original order of the input vector.
     */
    auto order(
        std::vector<asts::mixins::OrderableAst*> &&asts)
        -> std::vector<std::pair<std::string, asts::Ast*>>;

    auto order_args(
        std::vector<asts::mixins::OrderableAst*> &&args)
        -> std::vector<std::pair<std::string, asts::Ast*>>;

    auto order_params(
        std::vector<asts::mixins::OrderableAst*> &&params)
        -> std::vector<std::pair<std::string, asts::Ast*>>;
}
