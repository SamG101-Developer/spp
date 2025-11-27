module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_sym_info;
import llvm;


namespace spp::codegen {
    SPP_EXP_CLS struct LlvmTypeSymInfo {
        llvm::Type *llvm_type;
        llvm::Module *llvm_mod;
    };

    SPP_EXP_CLS struct LlvmVarSymInfo {
        llvm::Value *alloca;
    };
}
