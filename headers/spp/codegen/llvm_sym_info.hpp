#pragma once

#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>


namespace spp::codegen {
    struct LlvmTypeSymInfo {
        llvm::Type *llvm_type;
        llvm::Module *llvm_mod;
    };

    struct LlvmVarSymInfo {
        llvm::Value *alloca;
    };
}
