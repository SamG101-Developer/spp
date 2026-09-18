module;
#include <spp/macros.hpp>

export module spp.analyse.utils.builtins;
import spp.analyse.utils.cmp_utils;
import spp.asts.meta.compiler_meta_data;
import spp.codegen.llvm_ctx;
import spp.utils.functions;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::analyse::utils::builtins, struct LoweredFuncImpl);

/// A lowered function implementation is an implementation
/// written with LLVM IR directly. Typically this is to use
/// intrinsic functions, or non s++ expressible logic.
SPP_EXP_CLS struct spp::analyse::utils::builtins::LoweredFuncImpl {
  Function<void(
    ScopeManager *,
    FunctionPrototypeAst const *,
    meta::CompilerMetaData *,
    codegen::LlvmCtx *,
    llvm::Type *)> llvm_fn;
  Unique<cmp_utils::CmpFn> cmp_fn;
  Str name;
};

namespace spp::analyse::utils::builtins {
  /// Generate the mapping of all S++ functions tagged with
  /// "!intrinsic(name="some.ns.name")" to the llvm function
  /// generator, and optionally the c++ comptime function
  /// generator. This allows, given the intrinsic name, to
  /// lookup the function that produces the IR, and the
  /// function that computes the operation in the C++
  /// compile-time context.
  auto MakeBuiltinFuncMap() -> Map<Str, LoweredFuncImpl>;

  /// Create the static const map that will be used so that
  /// the lookup map only has to be generated once.
  SPP_EXP_CMP const auto kBuiltinFuncs = MakeBuiltinFuncMap();
}
