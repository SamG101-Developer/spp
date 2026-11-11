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

use_alias(spp::asts::meta, CompilerMetaData);

/// A lowered function implementation is an implementation written with LLVM IR
/// directly. Typically, this is to use intrinsic functions, or logic that is
/// not expressible in s++.
SPP_EXP_CLS struct spp::codegen::builtins::LoweredFnImpl {
  /// The function that upon calling, provides the llvm implementation via the
  /// llvm builder mechanic.
  Func<void(
    ScopeManager *, FunctionPrototypeAst const *, CompilerMetaData *, LlvmCtx *,
    llvm::Type *)> LlvmImpl;

  /// The wrapper around the c++ comptime executor for the intrinsic, should it
  /// exist (nullptr for no "cmp" implementation).
  Unique<analyse::utils::comp_time_intrinsics::CompTimeFn> CompTimeImpl;

  /// The intrinsics name, as seen in "!intrinsic(name="a.b.c")", which is used
  /// to index the builtin-functions map below.
  Str Name;

  /// The generic parameter whose destructor the lowering calls, if any ("T" for
  /// "drop"). No call site resolves that destructor, so each instantiation of
  /// the builtin has to make sure it exists.
  Str DropsGn;

  /// Lowers a direct call in place of the call, from the argument values, for a
  /// lowering that has to sit in the caller's own code, like a branch hint.
  /// Effectively a function that doesn't have even an IR body, but rather
  /// lowers to a call-site modifier.
  Func<llvm::Value*(Vec<llvm::Value*> const &, LlvmCtx *)> CallSiteImpl;
};

namespace spp::codegen::builtins {
  /// Generate the mapping of all S++ functions tagged with "!intrinsic" to the
  /// llvm function generator, and optionally the c++ comptime function
  /// generator.
  auto MakeBuiltinFnMap() -> Map<Str, LoweredFnImpl>;

  /// The static const map that will hold the lookup map, so that it only has to
  /// be generated once.
  SPP_EXP_FUN extern const Map<Str, LoweredFnImpl> kBuiltinFuncs;
}
