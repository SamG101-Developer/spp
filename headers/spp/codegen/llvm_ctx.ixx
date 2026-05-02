module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
}

namespace spp::codegen {
    SPP_EXP_CLS struct LLvmCtx;
    auto global_context = new llvm::LLVMContext();
}


SPP_EXP_CLS struct spp::codegen::LLvmCtx {
    llvm::LLVMContext *Context;
    Unique<llvm::Module> Module;
    llvm::IRBuilder<> Builder;
    std::map<Str, llvm::Constant*> GlobalConstants;
    bool InConstantContext = false;

    // Coroutine information.
    Vec<llvm::BasicBlock*> YieldContinuations;

    // Closure tracking information.
    llvm::Type *CurrentClosureType = nullptr;
    analyse::scopes::Scope *CurrentClosureScope = nullptr;

    // Loop black tracking information (for loop control flow).
    std::stack<llvm::BasicBlock*> LoopEndBBStack; // Allows breaking out of N loops.
    llvm::BasicBlock *LoopInnermostCondBB = nullptr; // For "skip" statements.

    LLvmCtx(LLvmCtx const &) = delete;
    LLvmCtx(LLvmCtx &&) noexcept = delete;
    auto operator=(LLvmCtx const &) -> LLvmCtx& = delete;
    auto operator=(LLvmCtx &&) noexcept -> LLvmCtx& = delete;

    LLvmCtx() :
        Context(global_context),
        Module(nullptr),
        Builder(*Context) {
    }

    static auto NewCtx(Str const &module_name) -> Unique<LLvmCtx> {
        auto ctx = MakeUnique<LLvmCtx>();
        ctx->Module = MakeUnique<llvm::Module>(module_name, *ctx->Context);
        ctx->Module->setTargetTriple(llvm::Triple("x86_64-pc-linux-gnu"));
        return ctx;
    }
};
