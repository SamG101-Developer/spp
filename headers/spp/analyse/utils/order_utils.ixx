module;
#include <spp/macros.hpp>

export module spp.analyse.utils.order_utils;
import spp.utils.types;
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
        spp::asts::utils::OrderableTag::kPositionalArg,
        spp::asts::utils::OrderableTag::kKeywordArg,
    };

    inline constexpr std::array PARAM_ORDER_ARR{
        spp::asts::utils::OrderableTag::kSelfParam,
        spp::asts::utils::OrderableTag::kRequiredParam,
        spp::asts::utils::OrderableTag::kOptionalParam,
        spp::asts::utils::OrderableTag::kVariadicParam,
    };

    /**
     * Return a list of items that are not in order. The order is provided by internal tags attached to the ASTs, as
     * they all inherit the @c OrderableAst mixin.
     * @param args The list of arguments to check the order of.
     * @param order The correct order of the tags.
     * @return The list of arguments that are out of order, paired with a string representation of their tag.
     */
    SPP_EXP_FUN auto DoOrder(
        Vec<asts::mixins::OrderableAst *> &&args,
        Vec<asts::utils::OrderableTag> order)
        -> Vec<Pair<Str, asts::Ast *>>;

    /**
     * The entry point into ordering arguments. This uses the internal order defined for function call arguments:
     * POSITIONAL -> KEYWORD.
     * @param args The list of arguments to check the order of.
     * @return The list of arguments that are out of order, paired with a string representation of their tag.
     */
    SPP_EXP_FUN auto DoOrderArgs(
        Vec<asts::mixins::OrderableAst *> &&args)
        -> Vec<Pair<Str, asts::Ast *>>;

    /**
     * The entry point into ordering parameters. This uses the internal order defined for function parameters:
     * SELF -> REQUIRED -> OPTIONAL -> VARIADIC.
     * @param params The list of parameters to check the order of.
     * @return The list of parameters that are out of order, paired with a string representation of their tag.
     */
    SPP_EXP_FUN auto DoOrderParams(
        Vec<asts::mixins::OrderableAst *> &&params)
        -> Vec<Pair<Str, asts::Ast *>>;
}
