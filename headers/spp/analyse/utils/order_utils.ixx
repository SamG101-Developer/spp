module;
#include <spp/macros.hpp>

export module spp.analyse.utils.order_utils;
import spp.asts._fwd;
import spp.asts.mixins.orderable_ast;
import std;


namespace spp::asts::mixins {
    SPP_EXP_CLS struct OrderableAst;
}


SPP_EXP_CLS namespace spp::analyse::utils::order_utils {
    /**
     * Return a list of items that are not in order. The order is provided by internal tags attached to the ASTs, as
     * they all inherit the @c OrderableAst mixin.
     * @param args The list of arguments to check the order of.
     * @param order The correct order of the tags.
     * @return The list of arguments that are out of order, paired with a string representation of their tag.
     */
    auto order(
        std::vector<asts::mixins::OrderableAst*> &&args,
        std::vector<asts::mixins::OrderableTag> order)
        -> std::vector<std::pair<std::string, asts::Ast*>>;

    /**
     * The entry point into ordering arguments. This uses the internal order defined for function call arguments:
     * POSITIONAL -> KEYWORD.
     * @param args The list of arguments to check the order of.
     * @return The list of arguments that are out of order, paired with a string representation of their tag.
     */
    auto order_args(
        std::vector<asts::mixins::OrderableAst*> &&args)
        -> std::vector<std::pair<std::string, asts::Ast*>>;

    /**
     * The entry point into ordering parameters. This uses the internal order defined for function parameters:
     * SELF -> REQUIRED -> OPTIONAL -> VARIADIC.
     * @param params The list of parameters to check the order of.
     * @return The list of parameters that are out of order, paired with a string representation of their tag.
     */
    auto order_params(
        std::vector<asts::mixins::OrderableAst*> &&params)
        -> std::vector<std::pair<std::string, asts::Ast*>>;
}

/// @endcond
