module spp.codegen.llvm_coros;
import spp.codegen.llvm_ctx;
import std;

auto spp::codegen::CreateLlvmGeneratorStateType(
  LlvmCtx const *ctx)
  -> llvm::Type* {
  // Create a specialised struct for LLVM that contains 2
  // fields - the "yield" slot, and the "send" slot. Simple
  // load/store from the "gen" expression and the "res"
  // postfix operator interact with this, + the "suspend"
  // and "resume" intrinsics.
  const auto llvm_yield_slot_type = GetLlvmGeneratorStateYieldSlotType(ctx);
  const auto llvm_send_slot_type = GetLlvmGeneratorStateSendSlotType(ctx);
  return llvm::StructType::get(*ctx->Context, {llvm_yield_slot_type, llvm_send_slot_type});
}

auto spp::codegen::GetLlvmGeneratorStateYieldSlotType(
  LlvmCtx const *ctx)
  -> llvm::Type* {
  // The yield slot type is the 64-bit integer type.
  const auto llvm_yield_slot_type = llvm::IntegerType::getInt64Ty(
    *ctx->Context);
  return llvm_yield_slot_type;
}

auto spp::codegen::GetLlvmGeneratorStateSendSlotType(
  LlvmCtx const *ctx)
  -> llvm::Type* {
  // The send slot type is the 64-bit integer type.
  const auto llvm_send_slot_type = llvm::IntegerType::getInt64Ty(
    *ctx->Context);
  return llvm_send_slot_type;
}

auto spp::codegen::GetLlvmGeneratorFrameAlign(
  LlvmCtx const *ctx)
  -> llvm::Value* {
  return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->Context), alignof(std::max_align_t));
}

auto spp::codegen::GetLlvmGeneratorSlotPtr(
  llvm::Value *const state,
  const LlvmGeneratorStateStructFields field,
  char const *const name,
  LlvmCtx *const ctx)
  -> llvm::Value* {
  return ctx->Builder.CreateStructGEP(
    CreateLlvmGeneratorStateType(ctx), state, static_cast<unsigned>(std::to_underlying(field)), name);
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
