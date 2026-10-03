module;
#include <spp/macros.hpp>

export module spp.analyse.utils.control_flow;
import spp.utils.types;
import std;
import sys;

use(spp::analyse::scopes, class ScopeManager);
use(spp::asts, struct StatementAst);
use(spp::asts::meta, struct CompilerMetaData);

namespace spp::analyse::utils::control_flow {
  /// Whether control never continues past a statement: it is
  /// written to leave ("ret", "exit", "skip", or a block/case
  /// every path of which does), or its analysed type is "!" (a
  /// call to "abort", a loop with no way out). A "let" diverges
  /// when its value does. The statement must be analysed.
  /// Todo: This will be the only function left, so move file?
  SPP_EXP_FUN auto Diverges(
    StatementAst &stmt,
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> bool;
}
