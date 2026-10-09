module;
#include <spp/macros.hpp>

export module spp.codegen.builtins;
import spp.analyse.utils.comp_time_intrinsics;
import spp.asts.meta.compiler_meta_data;
import spp.codegen.llvm_ctx;
import spp.utils.functions;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::codegen::builtins, struct LoweredFnImpl);

/// A lowered function implementation is an implementation
/// written with LLVM IR directly. Typically this is to use
/// intrinsic functions, or non s++ expressible logic.
SPP_EXP_CLS struct spp::codegen::builtins::LoweredFnImpl {
  Function<void(
    ScopeManager *,
    FunctionPrototypeAst const *,
    meta::CompilerMetaData *,
    codegen::LlvmCtx *,
    llvm::Type *)> LlvmImpl;
  Unique<analyse::utils::comp_time_intrinsics::CompTimeFn> CompTimeImpl;
  Str Name;

  /// The generic parameter whose destructor the lowering calls,
  /// if any ("T" for "drop" and "drop_in_place"). No call site
  /// resolves that destructor, so each instantiation of the
  /// builtin has to make sure it exists.
  Str DropsGn;
};

namespace spp::codegen::builtins {
  /// Generate the mapping of all S++ functions tagged with
  /// "!intrinsic(name="some.ns.name")" to the llvm function
  /// generator, and optionally the c++ comptime function
  /// generator. This allows, given the intrinsic name, to
  /// lookup the function that produces the IR, and the
  /// function that computes the operation in the C++
  /// compile-time context.
  auto MakeBuiltinFnMap() -> Map<Str, LoweredFnImpl>;

  /// Create the static const map that will be used so that
  /// the lookup map only has to be generated once.
  SPP_EXP_CMP const auto kBuiltinFuncs = MakeBuiltinFnMap();
}
