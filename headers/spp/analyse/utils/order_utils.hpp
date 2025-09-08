#pragma once

#include <spp/asts/_fwd.hpp>
#include <spp/asts/mixins/orderable_ast.hpp>

namespace spp::asts::mixins {
    struct OrderableAst;
}

namespace spp::analyse::utils::order_utils {
    auto order(
        std::vector<asts::mixins::OrderableAst*> &&args,
        std::vector<asts::mixins::OrderableTag> order)
        -> std::vector<std::pair<std::string, asts::Ast*>>;

    auto order_args(
        std::vector<asts::mixins::OrderableAst*> &&args)
        -> std::vector<std::pair<std::string, asts::Ast*>>;

    auto order_params(
        std::vector<asts::mixins::OrderableAst*> &&params)
        -> std::vector<std::pair<std::string, asts::Ast*>>;
}
