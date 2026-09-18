module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_func;
import spp.codegen.llvm_ctx;
import llvm;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct TypeAst);
use(spp::codegen, struct LlvmFuncWrapper);

/// This is used as a shared pointer, wrapping the internal
/// LLVM function, allowing it to be shared between cloned
/// functions, and a one-place update mechanism too.
SPP_EXP_CLS struct spp::codegen::LlvmFuncWrapper {
  llvm::Function *Target;
};

namespace spp::codegen {
  /// To fix cross-module declaration errors, functions declared
  /// in other modules need their declarations brought into
  /// the current LLVM module, so they're accessible in the
  /// individual TU. This function either adds the declaration
  /// into this module, or it is already here so no work needs
  /// to be done. Either way, the declaration is returned.
  SPP_EXP_FUN auto GetOrAddTargetIntoCurrentModule(
    llvm::Function const &target,
    llvm::Module &current_module)
    -> llvm::Function*;

  /// The global constant equivalent of the above function;
  /// "cmp" statements can be defined in one module, and accessed
  /// in another, so again we need to expose the declaration into
  /// the current TU. Defined with external linkage, so the
  /// definition still remains in one place.
  SPP_EXP_FUN auto GetOrAddGlobalIntoCurrentModule(
    llvm::GlobalVariable const &target,
    llvm::Module &current_module)
    -> llvm::GlobalVariable*;

  /// Get the module that code is currently being emitted into.
  /// This is the module owning the function that the builder
  /// is inserting into, which is not always "ctx->Module"; a
  /// generic substitution is declared by whichever module's
  /// walk reached it first, but the body is generated into the
  /// module that declared the base version of it. For example,
  /// all "Vec[U8]" functions, whilst initially used in the "Str"
  /// module, are defined in the "Vec" module.
  SPP_EXP_FUN auto GetEmissionModule(LlvmCtx const &ctx) -> llvm::Module*;

  /// Build the value that a function genuinely is, when used
  /// as one: the same "{ fn_ptr, env_ptr }" that a closure is.
  /// The pointer is to a thunk rather than to "target", as a
  /// call through the pair passes the environment first, and
  /// a plain function doesn't have a parameter for it. The
  /// thunk is emitted once per module, and reused after that.
  SPP_EXP_FUN auto BuildFunctionValue(llvm::Function const &target, LlvmCtx *ctx) -> llvm::Constant*;

  /// Like the variant coercion function, a value naming a
  /// function that going into a function type, is replaced by
  /// the pair for the overload, that analysis chose there.
  SPP_EXP_FUN auto CoerceToFunctionValue(
    llvm::Value *llvm_val,
    TypeRef const &target,
    TypeRef const &source,
    ScopeManager const &sm,
    LlvmCtx *ctx)
    -> llvm::Value*;
}
