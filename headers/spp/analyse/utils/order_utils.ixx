module;
#include <spp/macros.hpp>

export module spp.analyse.utils.order_utils;
import spp.asts.utils.orderable;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
}

namespace spp::asts::mixins {
    SPP_EXP_CLS struct OrderableAst;
}


namespace spp::analyse::utils::order_utils {
    inline constexpr std::array ARG_ORDER_ARR{
        spp::asts::utils::OrderableTag::POSITIONAL_ARG,
        spp::asts::utils::OrderableTag::KEYWORD_ARG,
    };


    inline constexpr std::array PARAM_ORDER_ARR{
        spp::asts::utils::OrderableTag::SELF_PARAM,
        spp::asts::utils::OrderableTag::REQUIRED_PARAM,
        spp::asts::utils::OrderableTag::OPTIONAL_PARAM,
        spp::asts::utils::OrderableTag::VARIADIC_PARAM,
    };

    /**
     * Return a list of items that are not in order. The order is provided by internal tags attached to the ASTs, as
     * they all inherit the @c OrderableAst mixin.
     * @param args The list of arguments to check the order of.
     * @param order The correct order of the tags.
     * @return The list of arguments that are out of order, paired with a string representation of their tag.
     */
    SPP_EXP_FUN auto order(
        std::vector<asts::mixins::OrderableAst*> &&args,
        std::vector<asts::utils::OrderableTag> order)
        -> std::vector<std::pair<std::string, asts::Ast*>>;

    /**
     * The entry point into ordering arguments. This uses the internal order defined for function call arguments:
     * POSITIONAL -> KEYWORD.
     * @param args The list of arguments to check the order of.
     * @return The list of arguments that are out of order, paired with a string representation of their tag.
     */
    SPP_EXP_FUN auto order_args(
        std::vector<asts::mixins::OrderableAst*> &&args)
        -> std::vector<std::pair<std::string, asts::Ast*>>;

    /**
     * The entry point into ordering parameters. This uses the internal order defined for function parameters:
     * SELF -> REQUIRED -> OPTIONAL -> VARIADIC.
     * @param params The list of parameters to check the order of.
     * @return The list of parameters that are out of order, paired with a string representation of their tag.
     */
    SPP_EXP_FUN auto order_params(
        std::vector<asts::mixins::OrderableAst*> &&params)
        -> std::vector<std::pair<std::string, asts::Ast*>>;
}
