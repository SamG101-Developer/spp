#include <spp/analyse/utils/order_utils.hpp>
#include <spp/asts/ast.hpp>
#include <spp/asts/mixins/orderable_ast.hpp>

#include <genex/to_container.hpp>
#include <genex/algorithms/position.hpp>
#include <genex/algorithms/sorted.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/transform.hpp>
#include <genex/views/zip.hpp>
#include <magic_enum/magic_enum.hpp>


inline const auto ARG_ORDER = std::vector{
    spp::asts::mixins::OrderableTag::POSITIONAL_ARG,
    spp::asts::mixins::OrderableTag::KEYWORD_ARG
};


inline const auto PARAM_ORDER = std::vector{
    spp::asts::mixins::OrderableTag::SELF_PARAM,
    spp::asts::mixins::OrderableTag::REQUIRED_PARAM,
    spp::asts::mixins::OrderableTag::OPTIONAL_PARAM,
    spp::asts::mixins::OrderableTag::VARIADIC_PARAM
};


auto spp::analyse::utils::order_utils::order(
    std::vector<asts::mixins::OrderableAst*> &&args,
    std::vector<asts::mixins::OrderableTag> order)
    -> std::vector<std::pair<std::string, asts::Ast*>> {
    // Tag each argument with its "order tag".
    auto tagged_args = args
        | genex::views::transform([](auto *x) { return std::make_pair(x->get_order_tag(), x); })
        | genex::to<std::vector>();

    // Sort the arguments based on the correct order.
    auto args_sorted = genex::algorithms::sorted(tagged_args, [&](auto &&arg_a, auto &&arg_b) {
        auto a = genex::algorithms::position(order, [&](auto x) { return x == std::get<0>(arg_a); });
        auto b = genex::algorithms::position(order, [&](auto x) { return x == std::get<0>(arg_b); });
        return a < b;
    });

    // Return arguments that are out of order. Todo: why "ast_cast" fails here?
    auto out_of_order = genex::views::zip(tagged_args, args_sorted)
        | genex::views::filter([](auto &&x) { return std::get<0>(x) != std::get<1>(x); })
        | genex::views::transform([](auto &&x) { return std::get<1>(x); })
        | genex::views::transform([](auto &&x) {
            return std::make_pair(
                std::string(magic_enum::enum_name(std::get<0>(x))),
                dynamic_cast<asts::Ast*>(std::get<1>(x)));
        })
        | genex::to<std::vector>();
    return out_of_order;
}


auto spp::analyse::utils::order_utils::order_args(
    std::vector<asts::mixins::OrderableAst*> &&args)
    -> std::vector<std::pair<std::string, asts::Ast*>> {
    // Call the generic order function with the argument order.
    return order(std::move(args), ARG_ORDER);
}


auto spp::analyse::utils::order_utils::order_params(
    std::vector<asts::mixins::OrderableAst*> &&params)
    -> std::vector<std::pair<std::string, asts::Ast*>> {
    // Call the generic order function with the parameter order.
    return order(std::move(params), PARAM_ORDER);
}
