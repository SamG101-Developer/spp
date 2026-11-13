module;
#include <spp/macros.hpp>

module spp.analyse.utils.order_utils;
import spp.asts.ast;
import spp.asts.mixins.orderable_ast;
import genex;
import magic_enum;
import std;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::asts, Ast);
use_alias(spp::asts::mixins, OrderableAst);

namespace spp::analyse::utils::order_utils {
  static const Vec kArgOrderArr{
    asts::utils::OrderableTag::kPositionalArg,
    asts::utils::OrderableTag::kKeywordArg,
  };
  static const Vec kParamOrderArr{
    asts::utils::OrderableTag::kSelfParam,
    asts::utils::OrderableTag::kRequiredParam,
    asts::utils::OrderableTag::kOptionalParam,
    asts::utils::OrderableTag::kVariadicParam,
  };

  static auto DoOrder(
    Vec<OrderableAst*> &&args,
    Vec<asts::utils::OrderableTag> const &order)
    -> Vec<Pair<Str, Ast*>>;
}

auto spp::analyse::utils::order_utils::DoOrder(
  Vec<OrderableAst*> &&args,
  Vec<asts::utils::OrderableTag> const &order)
  -> Vec<Pair<Str, Ast*>> {
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
      return MakePair(Str(magic_enum::enum_name(x.first)), dynamic_cast<Ast*>(x.second));
    })
    | genex::to<Vec>();
  return out_of_order;
}

auto spp::analyse::utils::order_utils::DoOrderArgs(
  Vec<OrderableAst*> &&args)
  -> Vec<Pair<Str, Ast*>> {
  // Call the generic order function with the argument order. A run of
  // positionals after the keywords is a variadic parameter's tail
  // ("P[A=S32, Bool, U8]", "f(a=1, ..xs)"), as in Python: left to the matching
  // of arguments to parameters, which knows whether each lands on the variadic
  // one.
  auto tail = args.size();
  while (tail > 0 and args[tail - 1]->GetOrderTag() == asts::utils::OrderableTag::kPositionalArg) { --tail; }
  const auto after_keyword = genex::any_of(args | genex::views::take(tail), [](auto const *x) {
    return x->GetOrderTag() == asts::utils::OrderableTag::kKeywordArg;
  });
  if (after_keyword) { args.resize(tail); }
  return DoOrder(std::move(args), kArgOrderArr);
}

auto spp::analyse::utils::order_utils::DoOrderParams(
  Vec<OrderableAst*> &&params)
  -> Vec<Pair<Str, Ast*>> {
  // Call the generic order function with the parameter order.
  return DoOrder(std::move(params), kParamOrderArr);
}
