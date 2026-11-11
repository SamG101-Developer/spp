module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_fn;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct TypeAst);
use(spp::codegen, struct LlvmFnWrapper);
use(spp::codegen, struct LlvmVarSymbolInfo);

/// This wrapper is used as a shared pointer, wrapping the internal LLVM
/// function, allowing it to be shared between cloned functions, and a
/// one-place update mechanism too.
SPP_EXP_CLS struct spp::codegen::LlvmFnWrapper {
  llvm::Function *Target;
};

namespace spp::codegen {
  /// To fix cross-module declaration errors, functions declared/defined in
  /// other modules need their declarations brought into the current LLVM
  /// module, so they're accessible in the individual TU. This function either
  /// adds the declaration into this module, or if it is already here no work
  /// needs to be done. Either way, the declaration is returned.
  SPP_EXP_FUN auto GetOrAddTargetIntoCurrentModule(
    llvm::Function const &target,
    llvm::Module &current_module) -> llvm::Function*;

  /// The global constant equivalent of the above function; "cmp" statements can
  /// be defined in one module, and accessed in another, so again we need to
  /// expose the declaration into the current TU. Defined with external linkage,
  /// so the definition still remains in one place.
  SPP_EXP_FUN auto GetOrAddGlobalIntoCurrentModule(
    llvm::GlobalVariable const &target,
    llvm::Module &current_module) -> llvm::GlobalVariable*;

  /// Read the global constant a "cmp" symbol is held in, from the module being
  /// emitted into, as the type it is read as.
  SPP_EXP_FUN auto LoadGlobalConstant(
    LlvmVarSymbolInfo const &info, Str const &name,
    LlvmCtx *ctx) -> llvm::Value*;

  /// Build the value that a function genuinely is, when used as one: the same
  /// "{ fn_ptr, env_ptr }" that a closure is for the unified functional type.
  /// The pointer is to a thunk rather than to "target", as a call through the
  /// pair passes the environment first, and a plain function doesn't have a
  /// parameter for it. The thunk is emitted once per module and function type,
  /// and reused after that.
  SPP_EXP_FUN auto BuildFnValue(
    llvm::Function const &target, TypeRef const &fn_sup,
    LlvmCtx const *ctx) -> llvm::Constant*;

  /// A closure's environment type is a header of an "i8" saying whether the
  /// environment owns its storage (heap => 1) or not (stack => 0) and a pointer
  /// to the function dropping its captures ("void(ptr env)", null when none
  /// needs dropping), then one field per capture.
  SPP_EXP_FUN auto ClosureEnvType(
    Vec<llvm::Type*> const &capture_tys, StrView name,
    LlvmCtx const *ctx) -> llvm::StructType*;

  /// Make a closure's environment of type "env_ty" - on the heap when it may
  /// outlive the frame making it, else on the stack - and record which in its
  /// header, with "drop_captures" (null when no captures needs dropping).
  SPP_EXP_FUN auto EmitNewClosureEnv(
    llvm::StructType *env_ty, bool on_heap, llvm::Function *drop_captures,
    StrView name, LlvmCtx *ctx) -> llvm::Value*;

  /// The field of a closure's environment holding capture "i".
  SPP_EXP_FUN auto ClosureEnvCaptureIndex(std::size_t i) -> unsigned;

  /// Free a closure's environment, if it has one, and it owns its storage: what
  /// consuming a "FunMov" does, as its body has consumed the captures.
  SPP_EXP_FUN auto EmitReleaseClosureEnv(
    llvm::Value *env, LlvmCtx *ctx) -> void;

  /// Drop a closure's captures through its header, then free its environment as
  /// "EmitReleaseClosureEnv" does: what dropping a closure that was never
  /// consumed does.
  SPP_EXP_FUN auto EmitDropClosureEnv(
    llvm::Value *env, LlvmCtx *ctx) -> void;
}
