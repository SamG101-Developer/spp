module;
#include <spp/macros.hpp>

export module spp.compiler.compiler_link;
import spp.codegen.llvm_ctx;
import spp.compiler.out_layout;
import spp.utils.types;
import std;

/// What happens once every module has its code: they are linked into one
/// module, optimised across the file boundaries the per-module walk leaves in
/// place, hardened, written out as an object, and linked against the ffi
/// runtimes the project's packages ship into an executable.
namespace spp::compiler {
  /// Copy every module in "ctxs" into one and run the optimization pipeline
  /// over that, written beside the per-module ir as "lto.ll" rather than
  /// replacing it; then, for a program (an "entry" point's linkage name, empty
  /// for a library), harden it, emit its object and link the executable. With
  /// "verify_only" (the unit tests), stop once the combined module is valid.
  /// True for a successful build.
  SPP_EXP_FUN auto LinkTimeOptimize(
    Vec<Unique<codegen::LlvmCtx>> const &ctxs, StrView entry,
    OutLayout const &out, unsigned opt_level, bool verify_only) -> bool;
}
