module;
#include <spp/macros.hpp>

export module spp.compiler.cfg;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct ModulePrototypeAst;
}

namespace spp::compiler::cfg {
  /// The keys a "!cfg" condition can give, as std's "cfg"
  /// annotation names its parameters: conditions are applied
  /// before anything is resolved, so its signature cannot be read.
  SPP_EXP_FUN auto CfgKeys() -> Vec<StrView> const&;

  /// Whether a "!cfg" value matches what the build has: one of
  /// its alternatives, separated by "|", or, after a leading "!",
  /// none of them.
  SPP_EXP_FUN auto CfgValueMatches(StrView condition, StrView actual) -> bool;

  /// Drop every member of the module, and of the classes and sup
  /// blocks in it, whose "!cfg" condition the target and build
  /// mode do not meet, answering where each was written: from
  /// its first annotation to its end, as source positions, for
  /// an editor to grey out. A condition with an unknown key or
  /// value is an error, raised here.
  SPP_EXP_FUN auto ApplyCfg(asts::ModulePrototypeAst &mod, StrView mode) -> Vec<Pair<std::size_t, std::size_t>>;
}
