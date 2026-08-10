module;
#include <spp/macros.hpp>

module spp.codegen.llvm_ctx;
import spp.codegen.llvm_coros;

SPP_MOD_BEGIN
spp::codegen::LLvmCtx::LLvmCtx() :
  Context(GLOBAL_CONTEXT),
  Module(nullptr),
  Builder(*Context),
  MF(nullptr) {
}

spp::codegen::LLvmCtx::~LLvmCtx() = default;

auto spp::codegen::LLvmCtx::NewCtx(Str const &module_name) -> Unique<LLvmCtx> {
  auto ctx = MakeUnique<LLvmCtx>();
  ctx->Module = MakeUnique<llvm::Module>(module_name, *ctx->Context);
  ctx->Module->setTargetTriple(llvm::Triple("x86_64-pc-linux-gnu"));
  return ctx;
}

SPP_MOD_END
