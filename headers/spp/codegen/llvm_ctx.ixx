module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_ctx;
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
    llvm::LLVMContext *context;
    std::unique_ptr<llvm::Module> module;
    llvm::IRBuilder<> builder;
    std::map<std::string, llvm::Constant*> global_constants;
    bool in_constant_context = false;

    // Coroutine information.
    std::vector<llvm::BasicBlock*> yield_continuations;

    // Closure tracking information.
    llvm::Type *current_closure_type = nullptr;
    analyse::scopes::Scope *current_closure_scope = nullptr;

    // Loop black tracking information (for loop control flow).
    std::stack<llvm::BasicBlock*> loop_end_bb_stack; // Allows breaking out of N loops.
    llvm::BasicBlock *loop_innermost_cond_bb = nullptr; // For "skip" statements.

    LLvmCtx(LLvmCtx const &) = delete;
    LLvmCtx(LLvmCtx &&) noexcept = delete;
    auto operator=(LLvmCtx const &) -> LLvmCtx& = delete;
    auto operator=(LLvmCtx &&) noexcept -> LLvmCtx& = delete;

    LLvmCtx() :
        context(global_context),
        module(nullptr),
        builder(*context) {
    }

    static auto new_ctx(std::string const &module_name) -> std::unique_ptr<LLvmCtx> {
        auto ctx = std::make_unique<LLvmCtx>();
        ctx->module = std::make_unique<llvm::Module>(module_name, *ctx->context);
        ctx->module->setTargetTriple(llvm::Triple("x86_64-pc-linux-gnu"));
        return ctx;
    }
};
