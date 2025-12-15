module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::codegen {
    SPP_EXP_CLS struct LLvmCtx;
    auto global_context = new llvm::LLVMContext();
}


SPP_EXP_CLS struct spp::codegen::LLvmCtx {
    llvm::LLVMContext *context;
    std::unique_ptr<llvm::Module> module;
    llvm::IRBuilder<> builder;
    std::map<std::string, llvm::Constant*> global_constants;
    bool in_constant_context = false;

    LLvmCtx(LLvmCtx const &) = delete;
    LLvmCtx(LLvmCtx &&) noexcept = delete;
    auto operator=(LLvmCtx const &) -> LLvmCtx& = delete;
    auto operator=(LLvmCtx &&) noexcept -> LLvmCtx& = delete;

    LLvmCtx() :
        context(global_context),
        module(nullptr),
        builder(llvm::IRBuilder(*context)) {
    }

    static auto new_ctx(std::string const &module_name) -> std::unique_ptr<LLvmCtx> {
        auto ctx = std::make_unique<LLvmCtx>();
        ctx->module = std::make_unique<llvm::Module>(module_name, *ctx->context);
        ctx->module->setTargetTriple(llvm::Triple("x86_64-pc-linux-gnu"));
        return ctx;
    }
};
