module spp.analyse.utils.order_utils;
import spp.asts.ast;
import spp.asts.mixins.orderable_ast;

import genex;
import magic_enum;
import std;


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


auto spp::analyse::utils::order_utils::order(
    std::vector<asts::mixins::OrderableAst*> &&args,
    std::vector<asts::utils::OrderableTag> order)
    -> std::vector<std::pair<std::string, asts::Ast*>> {
    // Tag each argument with its "order tag".
    auto tagged_args = args
        | genex::views::transform([](auto *x) { return std::make_pair(x->get_order_tag(), x); })
        | genex::to<std::vector>();

    // Sort the arguments based on the correct order.
    auto args_sorted = genex::sorted(tagged_args, [&](auto &&arg_a, auto &&arg_b) {
        auto a = genex::position(order, [&](auto x) { return x == std::get<0>(arg_a); });
        auto b = genex::position(order, [&](auto x) { return x == std::get<0>(arg_b); });
        return a < b;
    });

    // Return arguments that are out of order.
    auto out_of_order = genex::views::zip(tagged_args, args_sorted)
        | genex::views::filter([](auto &&x) { return std::get<0>(x) != std::get<1>(x); })
        | genex::views::transform([](auto &&x) { return std::get<1>(x); })
        | genex::views::transform([](auto &&x) { return std::make_pair(std::string(magic_enum::enum_name(std::get<0>(x))), dynamic_cast<asts::Ast*>(std::get<1>(x))); })
        | genex::to<std::vector>();
    return out_of_order;
}


auto spp::analyse::utils::order_utils::order_args(
    std::vector<asts::mixins::OrderableAst*> &&args)
    -> std::vector<std::pair<std::string, asts::Ast*>> {
    // Call the generic order function with the argument order.
    return order(
        std::move(args),
        std::vector(ARG_ORDER_ARR.begin(), ARG_ORDER_ARR.end())
    );
}


auto spp::analyse::utils::order_utils::order_params(
    std::vector<asts::mixins::OrderableAst*> &&params)
    -> std::vector<std::pair<std::string, asts::Ast*>> {
    // Call the generic order function with the parameter order.
    return order(
        std::move(params),
        std::vector(PARAM_ORDER_ARR.begin(), PARAM_ORDER_ARR.end())
    );
    return std::vector<std::pair<std::string, asts::Ast*>>{};
}
