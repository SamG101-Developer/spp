module;
#include <spp/macros.hpp>
#include <spp/codegen/llvm_passes.hpp>

module spp.codegen.llvm_ctx;
import spp.codegen.llvm_coros;

SPP_MOD_BEGIN
spp::codegen::LlvmCtx::LlvmCtx() :
  Context(GLOBAL_CONTEXT),
  Module(nullptr),
  Builder(*Context),
  MF(nullptr) {
}

spp::codegen::LlvmCtx::~LlvmCtx() = default;

auto spp::codegen::LlvmCtx::NewCtx(Str const &module_name) -> Unique<LlvmCtx> {
  auto ctx = MakeUnique<LlvmCtx>();
  ctx->Module = MakeUnique<llvm::Module>(module_name, *ctx->Context);
  ctx->Module->setModuleIdentifier(module_name);
  ctx->Module->setSourceFileName(module_name);
  ApplyTargetToModule(ctx->Module.get());
  return ctx;
}

SPP_MOD_END
