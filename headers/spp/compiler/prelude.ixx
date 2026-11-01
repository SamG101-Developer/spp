module;
#include <spp/macros.hpp>

export module spp.compiler.prelude;
import spp.utils.types;
import std;

namespace spp::compiler {
  /// The prelude contains types and symbols always imported into every module
  /// for ease-of-use. The only files they are not auto-imported into is the STD
  /// files, because of self-imports causing type clashes.
  /// Todo: We should probably fix this in the general case (if namespace ==
  ///  this module's scope as namespace, remove from prelude).
  SPP_EXP_FUN extern const Str kPrelude;
}
