#pragma once

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Module.h>


namespace spp::codegen {
    struct LlvmSymInfo {
        std::unique_ptr<llvm::StructType> llvm_struct_type;
        llvm::Module *llvm_mod;
    };
}
