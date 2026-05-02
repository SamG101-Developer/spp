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
    Vec<asts::mixins::OrderableAst*> &&args,
    Vec<asts::utils::OrderableTag> order)
    -> Vec<Pair<Str, asts::Ast*>> {
    // Tag each argument with its "order tag".
    auto tagged_args = args
        | genex::views::transform([](auto *x) { return MakePair(x->GetOrderTag(), x); })
        | genex::to<Vec>();

    // Sort the arguments based on the correct order.
    auto args_sorted = genex::sorted(tagged_args, [&](auto &&arg_a, auto &&arg_b) {
        auto a = genex::position(order, [&](auto x) { return x == arg_a.First; });
        auto b = genex::position(order, [&](auto x) { return x == arg_b.First; });
        return a < b;
    });

    // Return arguments that are out of order.
    auto out_of_order = genex::views::zip(tagged_args, args_sorted)
        | genex::views::filter([](auto &&x) { return std::get<0>(x) != std::get<1>(x); })
        | genex::views::transform([](auto &&x) { return std::get<1>(x); })
        | genex::views::transform([](auto &&x) { return MakePair(Str(magic_enum::enum_name(x.First)), dynamic_cast<asts::Ast*>(x.Second)); })
        | genex::to<Vec>();
    return out_of_order;
}

auto spp::analyse::utils::order_utils::order_args(
    Vec<asts::mixins::OrderableAst*> &&args)
    -> Vec<Pair<Str, asts::Ast*>> {
    // Call the generic order function with the argument order.
    return order(std::move(args), Vec(ARG_ORDER_ARR.begin(), ARG_ORDER_ARR.end()));
}

auto spp::analyse::utils::order_utils::order_params(
    Vec<asts::mixins::OrderableAst*> &&params)
    -> Vec<Pair<Str, asts::Ast*>> {
    // Call the generic order function with the parameter order.
    return order(std::move(params), Vec(PARAM_ORDER_ARR.begin(), PARAM_ORDER_ARR.end()));
}
