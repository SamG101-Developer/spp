module spp.codegen.llvm_coros;
import spp.codegen.llvm_ctx;
import std;

namespace {
  auto SlotStorageType(
    llvm::Type *const slot_type,
    spp::codegen::LlvmCtx const *ctx)
    -> llvm::Type* {
    return slot_type != nullptr and slot_type->isSized()
      ? slot_type
      : llvm::IntegerType::getInt64Ty(*ctx->Context);
  }
}

auto spp::codegen::CreateLlvmGeneratorStateType(
  llvm::Type *const yield_type,
  llvm::Type *const send_type,
  LlvmCtx const *ctx)
  -> llvm::StructType* {
  // Create a specialised struct for LLVM that contains 2
  // fields - the "yield" slot, and the "send" slot. Simple
  // load/store from the "gen" expression and the "res"
  // postfix operator interact with this, + the "suspend"
  // and "resume" intrinsics.
  return llvm::StructType::get(
    *ctx->Context,
    {SlotStorageType(yield_type, ctx), SlotStorageType(send_type, ctx)});
}

auto spp::codegen::GetLlvmGeneratorFrameAlign(
  LlvmCtx const *ctx)
  -> llvm::Value* {
  return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->Context), alignof(std::max_align_t));
}

auto spp::codegen::GetLlvmGeneratorSlotPtr(
  llvm::Value *const state,
  llvm::Type *const state_type,
  const LlvmGeneratorStateStructFields field,
  char const *const name,
  LlvmCtx *const ctx)
  -> llvm::Value* {
  return ctx->Builder.CreateStructGEP(
    state_type, state, static_cast<unsigned>(std::to_underlying(field)), name);
}

auto spp::codegen::GetLlvmGeneratorStateFromHandle(
  llvm::Value *const handle,
  LlvmCtx *const ctx)
  -> llvm::Value* {
  return ctx->Builder.CreateIntrinsic(
    llvm::Intrinsic::coro_promise, {}, {handle, GetLlvmGeneratorFrameAlign(ctx), ctx->Builder.getFalse()}, {},
    "gen.state");
}

auto spp::codegen::EmitLlvmGeneratorSuspend(
  const bool is_final,
  llvm::BasicBlock *const parked_bb,
  llvm::BasicBlock *const cleanup_bb,
  Str const &suspend_name,
  Str const &resume_name,
  LlvmCtx *const ctx)
  -> llvm::BasicBlock* {
  const auto suspend_res = ctx->Builder.CreateIntrinsic(
    llvm::Intrinsic::coro_suspend, {},
    {llvm::ConstantTokenNone::get(*ctx->Context), ctx->Builder.getInt1(is_final)}, {}, suspend_name);

  const auto i8_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto resume_bb = llvm::BasicBlock::Create(
    *ctx->Context, resume_name, ctx->Builder.GetInsertBlock()->getParent());

  const auto suspend_switch = ctx->Builder.CreateSwitch(suspend_res, parked_bb, 2);
  suspend_switch->addCase(llvm::ConstantInt::get(i8_ty, 0), resume_bb);
  suspend_switch->addCase(llvm::ConstantInt::get(i8_ty, 1), cleanup_bb);

  ctx->Builder.SetInsertPoint(resume_bb);
  return resume_bb;
}
