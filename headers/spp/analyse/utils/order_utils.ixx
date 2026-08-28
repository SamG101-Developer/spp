module;
#include <spp/macros.hpp>

export module spp.analyse.utils.order_utils;
import spp.asts.utils.orderable;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct Ast;
}

namespace spp::asts::mixins {
  SPP_EXP_CLS struct OrderableAst;
}

namespace spp::analyse::utils::order_utils {
  /**
   * The entry point into ordering arguments. This uses the internal order defined for function call arguments:
   * POSITIONAL -> KEYWORD.
   * @param args The list of arguments to check the order of.
   * @return The list of arguments that are out of order, paired with a string representation of their tag.
   */
  SPP_EXP_FUN auto DoOrderArgs(
    Vec<asts::mixins::OrderableAst*> &&args)
    -> Vec<Pair<Str, asts::Ast*>>;

  /**
   * The entry point into ordering parameters. This uses the internal order defined for function parameters:
   * SELF -> REQUIRED -> OPTIONAL -> VARIADIC.
   * @param params The list of parameters to check the order of.
   * @return The list of parameters that are out of order, paired with a string representation of their tag.
   */
  SPP_EXP_FUN auto DoOrderParams(
    Vec<asts::mixins::OrderableAst*> &&params)
    -> Vec<Pair<Str, asts::Ast*>>;
}
