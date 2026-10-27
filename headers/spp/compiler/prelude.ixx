module;
#include <spp/macros.hpp>

export module spp.compiler.prelude;
import spp.utils.types;
import std;

namespace spp::compiler {
  /// The prelude contains types and symbols always imported
  /// into every module for ease-of-use.
  SPP_EXP_FUN extern const Str kPrelude;
}
