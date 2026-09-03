module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS class ScopeManager;
}

namespace spp::codegen {
  SPP_EXP_CLS struct LlvmCtx;
  SPP_EXP_CLS struct LlvmGenerator;
  auto GLOBAL_CONTEXT = new llvm::LLVMContext();
}

SPP_EXP_CLS struct spp::codegen::LlvmCtx {
  // General context information.
  llvm::LLVMContext *Context;
  analyse::scopes::ScopeManager const *Sm = nullptr;
  Unique<llvm::Module> Module;
  llvm::IRBuilder<> Builder;
  bool InConstantContext = false;

  // Coroutine information.
  std::map<llvm::Value*, Unique<LlvmGenerator>> LlvmGenerators;

  // Closure tracking information.
  llvm::Type *CurrentClosureType = nullptr;
  analyse::scopes::Scope *CurrentClosureScope = nullptr;

  LlvmCtx(LlvmCtx const &) = delete;
  LlvmCtx(LlvmCtx &&) noexcept = delete;
  auto operator=(LlvmCtx const &) -> LlvmCtx& = delete;
  auto operator=(LlvmCtx &&) noexcept -> LlvmCtx& = delete;

  LlvmCtx();
  ~LlvmCtx();
  static auto NewCtx(Str const &module_name) -> Unique<LlvmCtx>;
};
