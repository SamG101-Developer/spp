module;
#include <spp/macros.hpp>

export module spp.compiler.prelude;
import spp.utils.types;
import std;

namespace spp::compiler {
  /// The prelude contains types and symbols always imported into every module
  /// for ease-of-use.
  SPP_EXP_FUN extern const Str kPrelude;

  /// The prelude for the module in namespace "ns_parts": every line but those
  /// importing from that namespace, which would import the module's own
  /// definitions into itself ("std::string" gets all but "std::string::Str").
  SPP_EXP_FUN auto PreludeFor(Vec<Str> const &ns_parts) -> Str;
}
