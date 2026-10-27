module;
#include <spp/macros.hpp>

module spp.codegen.llvm_ctx;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_defer;
import spp.codegen.llvm_target;

SPP_MOD_BEGIN
spp::codegen::LlvmCtx::LlvmCtx(
  llvm::LLVMContext &context) :
  Context(&context),
  Module(nullptr),
  Builder(*Context),
  Deferred(MakeUnique<DeferredReached>()) {
}

spp::codegen::LlvmCtx::~LlvmCtx() = default;

auto spp::codegen::LlvmCtx::NewCtx(Str const &module_name, llvm::LLVMContext &context) -> Unique<LlvmCtx> {
  auto ctx = MakeUnique<LlvmCtx>(context);
  ctx->Module = MakeUnique<llvm::Module>(module_name, *ctx->Context);
  ctx->Module->setModuleIdentifier(module_name);
  ctx->Module->setSourceFileName(module_name);
  ApplyTargetToModule(ctx->Module.get());
  return ctx;
}

SPP_MOD_END
