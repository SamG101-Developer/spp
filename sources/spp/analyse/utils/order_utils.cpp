module spp.analyse.utils.order_utils;
import spp.asts.ast;
import spp.asts.mixins.orderable_ast;
import genex;
import magic_enum;
import std;

namespace spp::analyse::utils::order_utils {
  namespace {
    constexpr std::array kArgOrderArr{
      spp::asts::utils::OrderableTag::kPositionalArg,
      spp::asts::utils::OrderableTag::kKeywordArg,
    };

    constexpr std::array kParamOrderArr{
      spp::asts::utils::OrderableTag::kSelfParam,
      spp::asts::utils::OrderableTag::kRequiredParam,
      spp::asts::utils::OrderableTag::kOptionalParam,
      spp::asts::utils::OrderableTag::kVariadicParam,
    };

    auto DoOrder(
      Vec<asts::mixins::OrderableAst*> &&args,
      Vec<asts::utils::OrderableTag> const &order)
      -> Vec<Pair<Str, asts::Ast*>> {
      // Tag each argument with its "order tag".
      auto tagged_args = args
        | genex::views::transform([](auto *x) { return MakePair(x->GetOrderTag(), x); })
        | genex::to<Vec>();

      // Sort the arguments based on the correct order.
      auto args_sorted = genex::sorted(tagged_args, [&](auto &&arg_a, auto &&arg_b) {
        auto a = genex::position(order, [&](auto x) { return x == arg_a.first; });
        auto b = genex::position(order, [&](auto x) { return x == arg_b.first; });
        return a < b;
      });

      // Return arguments that are out of order.
      auto out_of_order = genex::views::zip(tagged_args, args_sorted)
        | genex::to<Vec>()
        | genex::views::filter([](auto &&x) { return spp::get<0>(x) != spp::get<1>(x); })
        | genex::views::transform([](auto &&x) { return spp::get<1>(x); })
        | genex::views::transform([](auto &&x) {
          return MakePair(Str(magic_enum::enum_name(x.first)), dynamic_cast<asts::Ast*>(x.second));
        })
        | genex::to<Vec>();
      return out_of_order;
    }
  }
}

auto spp::analyse::utils::order_utils::DoOrderArgs(
  Vec<asts::mixins::OrderableAst*> &&args)
  -> Vec<Pair<Str, asts::Ast*>> {
  // Call the generic order function with the argument order.
  return DoOrder(std::move(args), ARG_ORDER_ARR);
}

auto spp::analyse::utils::order_utils::DoOrderParams(
  Vec<asts::mixins::OrderableAst*> &&params)
  -> Vec<Pair<Str, asts::Ast*>> {
  // Call the generic order function with the parameter order.
  return DoOrder(std::move(params), PARAM_ORDER_ARR);
}
