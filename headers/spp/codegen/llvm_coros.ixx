module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_coros;
import spp.utils.types;
import llvm;

use(spp::codegen, struct LlvmGenerator);
use(spp::codegen, struct LlvmCtx);
use(spp::codegen, enum class LlvmGeneratorStateStructFields);

SPP_EXP_CLS struct spp::codegen::LlvmGenerator {
  llvm::Value *Handle;
  llvm::Value *State;
  llvm::BasicBlock *CleanupBlock;
  llvm::BasicBlock *SuspendBlock;
  llvm::BasicBlock *FinalBlock;
};

SPP_EXP_CLS enum class spp::codegen::LlvmGeneratorStateStructFields {
  YIELD_SLOT = 0,
  SEND_SLOT = 1,
};

namespace spp::codegen {
  /// The struct a generator yields and receives through (the
  /// coroutines "promise") which is one field per direction,
  /// in the types that direction actually carries (yield type,
  /// send type).
  SPP_EXP_FUN auto CreateLlvmGeneratorStateType(
    llvm::Type *yield_type,
    llvm::Type *send_type,
    LlvmCtx const *ctx)
    -> llvm::StructType*;

  /// The alignment a coroutine frame and its promise are built
  /// to. One constant rather than one per call site, because
  /// "llvm.coro.id" and every "llvm.coro.promise reading that
  /// frame back have to agree on it: consistency.
  SPP_EXP_FUN auto GetLlvmGeneratorFrameAlign(LlvmCtx const *ctx) -> llvm::Value*;

  /// Get the address of one of the slots on the generator
  /// state, ie get the yield slot or get the send slot.
  SPP_EXP_FUN auto GetLlvmGeneratorSlotPtr(
    llvm::Value *state,
    llvm::Type *state_type,
    LlvmGeneratorStateStructFields field,
    char const *name,
    LlvmCtx *ctx)
    -> llvm::Value*;

  /// Get the state of an llvm generator from its value handle.
  /// Read the state back out of the frame.
  SPP_EXP_FUN auto GetLlvmGeneratorStateFromHandle(llvm::Value *handle, LlvmCtx *ctx) -> llvm::Value*;

  /// Emit a suspension point, and leave the builder inserting
  /// into the block that control returns to, when the coroutine
  /// is resumed from it. Uses the "llvm.coro.suspend" intrinsic.
  SPP_EXP_FUN auto EmitLlvmGeneratorSuspend(
    bool is_final,
    llvm::BasicBlock *parked_bb,
    llvm::BasicBlock *cleanup_bb,
    Str const &suspend_name,
    Str const &resume_name,
    LlvmCtx *ctx)
    -> llvm::BasicBlock*;
}
