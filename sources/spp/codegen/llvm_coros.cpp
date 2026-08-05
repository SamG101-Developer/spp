module spp.codegen.llvm_coros;
import spp.codegen.llvm_ctx;

auto spp::codegen::CreateLlvmGeneratorStateType(
  LLvmCtx const *ctx)
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
  LLvmCtx const *ctx)
  -> llvm::Type* {
  // The yield slot type is the 64-bit integer type.
  const auto llvm_yield_slot_type = llvm::IntegerType::getInt64Ty(
    *ctx->Context);
  return llvm_yield_slot_type;
}

auto spp::codegen::GetLlvmGeneratorStateSendSlotType(
  LLvmCtx const *ctx)
  -> llvm::Type* {
  // The send slot type is the 64-bit integer type.
  const auto llvm_send_slot_type = llvm::IntegerType::getInt64Ty(
    *ctx->Context);
  return llvm_send_slot_type;
}
