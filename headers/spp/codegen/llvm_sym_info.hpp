#pragma once

#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>


namespace spp::codegen {
    struct LlvmSymInfo {
        llvm::Type *llvm_type;
        llvm::Module *llvm_mod;
    };
}
