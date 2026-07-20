module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_sym_info;
import ankerl;
import llvm;
import std;

namespace spp::codegen {
    SPP_EXP_CLS struct LlvmTypeSymInfo {
        llvm::Type *LlvmType = nullptr;
        llvm::Module *LlvmMod = nullptr;
        ankerl::unordered_dense::map<std::size_t, std::size_t> FieldIndexMap;
    };

    SPP_EXP_CLS struct LlvmVarSymInfo {
        llvm::Value *Alloca = nullptr;
    };
}
