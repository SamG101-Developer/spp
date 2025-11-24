#pragma once
#include <map>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>


namespace spp::codegen {
    struct LLvmCtx;
}


struct spp::codegen::LLvmCtx {
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
    llvm::IRBuilder<> builder;
    std::map<std::string, llvm::Constant*> global_constants;
};
