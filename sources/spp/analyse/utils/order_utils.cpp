module spp.analyse.utils.order_utils;
import spp.asts.ast;
import spp.asts.mixins.orderable_ast;
import magic_enum;
import std;

inline constexpr std::array kArgOrderArr{
    spp::asts::utils::OrderableTag::kPositionalArg,
    spp::asts::utils::OrderableTag::kKeywordArg,
};

inline constexpr std::array kParamOrderArr{
    spp::asts::utils::OrderableTag::kSelfParam,
    spp::asts::utils::OrderableTag::kRequiredParam,
    spp::asts::utils::OrderableTag::kOptionalParam,
    spp::asts::utils::OrderableTag::kVariadicParam,
};

auto spp::analyse::utils::order_utils::DoOrder(
    Vec<asts::mixins::OrderableAst*> &&args,
    Vec<asts::utils::OrderableTag> order)
    -> Vec<Pair<Str, asts::Ast*>> {
    // Tag each argument with its "order tag".
    auto tagged_args = args
        | std::views::transform([](auto *x) { return MakePair(x->GetOrderTag(), x); })
        | std::ranges::to<Vec>();
    auto args_sorted = tagged_args; // Copy

    // Sort the arguments based on the correct order.
    std::ranges::sort(args_sorted, [&](auto &&arg_a, auto &&arg_b) {
        auto a = std::ranges::find(order, arg_a.First) - order.begin();
        auto b = std::ranges::find(order, arg_b.First) - order.begin();
        return a < b;
    });

    // Return arguments that are out of order.
    auto tmp = std::ranges::views::zip(tagged_args, args_sorted);
    auto out_of_order = tmp
        | std::views::filter([](auto &&x) { return std::get<0>(x) != std::get<1>(x); })
        | std::views::transform([](auto &&x) { return std::get<1>(x); })
        | std::views::transform([](auto &&x) { return MakePair(Str(magic_enum::enum_name(x.First)), dynamic_cast<asts::Ast*>(x.Second)); })
        | std::ranges::to<Vec>();
    return out_of_order;
}

auto spp::analyse::utils::order_utils::DoOrderArgs(
    Vec<asts::mixins::OrderableAst*> &&args)
    -> Vec<Pair<Str, asts::Ast*>> {
    // Call the generic order function with the argument order.
    return DoOrder(std::move(args), Vec(ARG_ORDER_ARR.begin(), ARG_ORDER_ARR.end()));
}

auto spp::analyse::utils::order_utils::DoOrderParams(
    Vec<asts::mixins::OrderableAst*> &&params)
    -> Vec<Pair<Str, asts::Ast*>> {
    // Call the generic order function with the parameter order.
    return DoOrder(std::move(params), Vec(PARAM_ORDER_ARR.begin(), PARAM_ORDER_ARR.end()));
}
