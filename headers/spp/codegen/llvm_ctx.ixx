module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_ctx;
import llvm;
import std;


namespace spp::codegen {
    SPP_EXP struct LLvmCtx;
}


SPP_EXP struct spp::codegen::LLvmCtx {
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
    llvm::IRBuilder<> builder;
    std::map<std::string, llvm::Constant*> global_constants;
};
