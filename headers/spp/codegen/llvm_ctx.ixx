module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::codegen, struct LlvmCtx);
use(spp::codegen, struct LlvmGenerator);

namespace spp::codegen {
  auto GLOBAL_CONTEXT = new llvm::LLVMContext();
}

/// The LLVM context information used throughout stage 10 and
/// 11 of the compiler's code generation.
SPP_EXP_CLS struct spp::codegen::LlvmCtx {
  /// The LLVM context object.
  llvm::LLVMContext *Context;

  /// The scope manager currently in use.
  ScopeManager const *Sm = nullptr;

  /// The LLVM module being emitted into.
  Unique<llvm::Module> Module;

  /// The LLVM builder currently in use.
  llvm::IRBuilder<> Builder;

  /// Whether we are generating into compile-time context or
  /// not - some literals have better generation.
  bool InConstantContext = false;

  /// A list of coroutine handles and their associated
  /// generators.
  Map<llvm::Value*, Unique<LlvmGenerator>> LlvmGenerators;

  /// Track the current closure type. Todo: Does this work
  /// with nested closures? I'd doubt it.
  llvm::Type *CurrentClosureType = nullptr;

  /// The current closure scope. Todo: Same issue as above.
  Scope *CurrentClosureScope = nullptr;

  /// Build a new context from the module name that is about
  /// to run stage 10/11,
  static auto NewCtx(Str const &module_name) -> Unique<LlvmCtx>;

  LlvmCtx();
  LlvmCtx(LlvmCtx const &) = delete;
  LlvmCtx(LlvmCtx &&) noexcept = delete;
  auto operator=(LlvmCtx const &) -> LlvmCtx& = delete;
  auto operator=(LlvmCtx &&) noexcept -> LlvmCtx& = delete;
  ~LlvmCtx();
};
