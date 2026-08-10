module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_coros;
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
}

SPP_EXP_CLS struct spp::codegen::LlvmGenerator {
  llvm::Value *Handle;
  llvm::Value *State;
  llvm::BasicBlock *CleanupBlock;
  llvm::BasicBlock *SuspendBlock;
};
