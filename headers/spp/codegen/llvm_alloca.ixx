module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_alloca;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;

namespace spp::codegen {
    /**
     * Create a stack slot in the entry block of the function currently being generated, rather than at the builder's
     * current insert point. Every fixed-size allocation must go through this function, for two reasons:
     * * llvm's @c mem2reg / @c SROA passes only promote entry block allocas into registers.
     * * an alloca emitted inside a loop body executes once per iteration, growing the stack until the function returns.
     * @param[in] type The llvm type to allocate a slot for.
     * @param[in] name The name to give the allocation in the generated ir.
     * @param[in] ctx The llvm context whose builder identifies the function being generated into.
     * @return The allocation instruction, placed at the top of the function's entry block.
     */
    SPP_EXP_FUN auto llvm_entry_alloca(
        llvm::Type *type,
        Str const &name,
        LLvmCtx *ctx)
        -> llvm::AllocaInst*;
}
