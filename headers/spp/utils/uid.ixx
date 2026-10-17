module;
#include <spp/macros.hpp>

export module spp.utils.uid;
import spp.utils.types;
import std;

namespace spp::utils {
  /// A name nothing else in this compile holds, for the temporaries and mock types a desugaring introduces.
  ///
  /// It is a counter and nothing else. It used to carry the address of the ast that asked for it, which added no
  /// uniqueness the counter did not already give - a counter never repeats - and cost reproducibility: the same
  /// project compiled twice produced different names, so no verification mode could compare two runs without
  /// scrubbing pointers out of its own inputs first.
  ///
  /// The counter is still per-process and per-call-order, so it is reproducible for one compile repeated and not for
  /// a compile that analyses a different amount of code first. Incrementality needs identity that survives that too,
  /// which is a deeper change than this one.
  SPP_EXP_FUN auto Uid() -> Str;
}
