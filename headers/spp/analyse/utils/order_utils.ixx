module;
#include <spp/macros.hpp>

export module spp.analyse.utils.order_utils;
import spp.asts.utils.orderable;
import spp.utils.types;
import std;

use(spp::asts::mixins, struct OrderableAst);
use(spp::asts, struct Ast);

namespace spp::analyse::utils::order_utils {
  /// Order arguments in the order [positional, keyword]. Return
  /// an out-of-order ast and a string defining why, that the
  /// error system can report with. Uses an internal core function.
  SPP_EXP_FUN auto DoOrderArgs(Vec<OrderableAst*> &&args) -> Vec<Pair<Str, Ast*>>;

  /// Order parameters in the order [self, required, optional, variadic].
  /// Return an out-of-order ast and a string defining why, that
  /// the error system can report with. Uses an internal core
  /// function.
  SPP_EXP_FUN auto DoOrderParams(Vec<OrderableAst*> &&params) -> Vec<Pair<Str, Ast*>>;
}
