#pragma once

#ifndef NDEBUG
#define VALIDATE_LLVM                                                              \
  {                                                                                \
    if (ctx != nullptr and ctx->Builder.GetInsertBlock() != nullptr and            \
        ctx->Builder.GetInsertBlock()->getParent() != nullptr) {                   \
      const auto validate_func = ctx->Builder.GetInsertBlock()->getParent();       \
      auto broken = llvm::verifyFunction(*validate_func, &llvm::errs());           \
      if (broken) {                                                                \
        llvm::errs()                                                               \
          << "\n\nVerification failure in function '"                              \
          << (validate_func->hasName() ? validate_func->getName() : "<anonymous>") \
          << "'.";                                                                 \
      }                                                                            \
    }                                                                              \
  }
#else
#define VALIDATE_LLVM
#endif
