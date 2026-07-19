module spp.codegen.llvm_alloca;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;


auto spp::codegen::llvm_entry_alloca(
    llvm::Type *const type,
    Str const &name,
    LLvmCtx *const ctx)
    -> llvm::AllocaInst* {
    // Insert at the very top of the entry block, ahead of any allocas already placed there.
    const auto func = ctx->Builder.GetInsertBlock()->getParent();
    const auto entry = &func->getEntryBlock();
    auto entry_builder = llvm::IRBuilder(entry, entry->begin());
    return entry_builder.CreateAlloca(type, nullptr, name);
}
