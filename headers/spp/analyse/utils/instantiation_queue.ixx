module;
#include <spp/macros.hpp>

export module spp.analyse.utils.instantiation_queue;
import spp.utils.types;
import std;

use(spp::asts, struct FunctionPrototypeAst);

/// The record of which function templates have had a generic
/// substitution registered against them that hasn't been
/// processed yet. Instantiations are discovered lazily, by
/// analysing function calls to generic function prototypes,
/// such as "1 + 2" creating "SizedInteger[32, true]::add",
/// whose body is analysed and in turn creates the overload
/// "intrinsics::add[T=S32]". All the instantiations are
/// recorded, and then drained to a fixed point, allows for
/// the set to be complete before anything downstream depends
/// on anything from the set.
namespace spp::analyse::utils::instantiation_queue {
  /// Record that a "fn_template" has a substitution needing
  /// processing. This is a new generically-instantiated
  /// version of a function prototype. If a template is
  /// re-queued, it is a no-op.
  SPP_EXP_FUN auto Enqueue(FunctionPrototypeAst *fn_template) -> void;

  /// Take the next template waiting to be processed, in the
  /// order they were recorded. This ordering, whilst not
  /// required for the overall processing, allows for errors
  /// in one instantiation to be reported against the genuine
  /// call for it rather than an unrelated later one.
  SPP_EXP_FUN auto Pop() -> FunctionPrototypeAst*;

  /// Drop everything waiting. Called between compilations,
  /// because the record outlives any one of them; prevents
  /// sharing state or memory leaks.
  SPP_EXP_FUN auto Clear() -> void;
}
