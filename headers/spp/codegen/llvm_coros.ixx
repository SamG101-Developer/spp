module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_coros;
import spp.utils.types;
import llvm;

namespace spp::codegen {
  SPP_EXP_CLS struct LlvmGenerator;
  SPP_EXP_CLS struct LlvmCtx;

  SPP_EXP_CLS enum class LlvmGeneratorStateStructFields {
    YIELD_SLOT = 0,
    SEND_SLOT = 1,
  };

  SPP_EXP_FUN auto CreateLlvmGeneratorStateType(
    LlvmCtx const *ctx)
    -> llvm::Type*;

  SPP_EXP_FUN auto GetLlvmGeneratorStateYieldSlotType(
    LlvmCtx const *ctx)
    -> llvm::Type*;

  SPP_EXP_FUN auto GetLlvmGeneratorStateSendSlotType(
    LlvmCtx const *ctx)
    -> llvm::Type*;

  /**
   * The alignment a coroutine frame and its promise are built to. One constant rather than one per call site, because
   * @c llvm.coro.id and every @c llvm.coro.promise reading that frame back have to agree on it.
   * @param[in] ctx The LLVM context containing all codegen info.
   * @return The alignment, as the @c i32 the coroutine intrinsics take.
   */
  SPP_EXP_FUN auto GetLlvmGeneratorFrameAlign(
    LlvmCtx const *ctx)
    -> llvm::Value*;

  /**
   * The address of one slot of the generator state @p state points at.
   *
   * @n
   * The slots are fields of a struct, so reaching one is a struct GEP: the source element type has to be the struct
   * and the field index an @c i32 . Indexing with the slot's own type instead produces
   * @code getelementptr i64, ptr %state, i64 0, i64 N @endcode , which asks to index into an @c i64 - not an
   * aggregate, so not a valid GEP.
   * @param[in] state The generator state object.
   * @param[in] field Which slot to reach.
   * @param[in] name Name for the resulting address.
   * @param[in] ctx The LLVM context containing all codegen info.
   * @return The address of the slot.
   */
  SPP_EXP_FUN auto GetLlvmGeneratorSlotPtr(
    llvm::Value *state,
    LlvmGeneratorStateStructFields field,
    char const *name,
    LlvmCtx *ctx)
    -> llvm::Value*;

  /**
   * The generator state living inside the frame @p handle refers to. A generator that arrived as a value carries only
   * its handle, so the state it yields through has to be read back out of the frame rather than remembered.
   * @param[in] handle The coroutine handle.
   * @param[in] ctx The LLVM context containing all codegen info.
   * @return The generator state object.
   */
  SPP_EXP_FUN auto GetLlvmGeneratorStateFromHandle(
    llvm::Value *handle,
    LlvmCtx *ctx)
    -> llvm::Value*;

  /**
   * Emit a suspend point, and leave the builder inserting into the block control returns to when the coroutine is
   * resumed from it.
   *
   * @n
   * The @c i8 that @c llvm.coro.suspend produces is what makes this a suspend point, and llvm's switch ABI requires
   * it to drive a switch rather than be tested: 0 resumes, 1 destroys, and the default parks. Without the switch the
   * coroutine passes see no suspend point to split the function on, and flatten it instead.
   * @param[in] is_final Whether this is the final suspend - the one a completed coroutine parks on, and the only one
   * @c llvm.coro.done reads as finished.
   * @param[in] parked_bb Where control goes when the coroutine parks, which is the switch's default.
   * @param[in] cleanup_bb Where control goes to destroy the frame.
   * @param[in] suspend_name Name for the suspend value.
   * @param[in] resume_name Name for the block resumption returns to.
   * @param[in] ctx The LLVM context containing all codegen info.
   * @return The resume block, which the builder is left inserting into.
   */
  SPP_EXP_FUN auto EmitLlvmGeneratorSuspend(
    bool is_final,
    llvm::BasicBlock *parked_bb,
    llvm::BasicBlock *cleanup_bb,
    Str const &suspend_name,
    Str const &resume_name,
    LlvmCtx *ctx)
    -> llvm::BasicBlock*;
}

SPP_EXP_CLS struct spp::codegen::LlvmGenerator {
  llvm::Value *Handle;
  llvm::Value *State;
  llvm::BasicBlock *CleanupBlock;
  llvm::BasicBlock *SuspendBlock;
};
