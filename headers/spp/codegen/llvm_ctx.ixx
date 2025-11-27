module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_ctx;
import llvm;
import std;


namespace spp::codegen {
    SPP_EXP_CLS struct LLvmCtx;
}


SPP_EXP_CLS struct spp::codegen::LLvmCtx {
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
    llvm::IRBuilder<> builder;
    std::map<std::string, llvm::Constant*> global_constants;
};
