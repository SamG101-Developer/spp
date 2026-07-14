module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_sym_info;
import spp.utils.types;
import llvm;

namespace spp::codegen {
    SPP_EXP_CLS struct LlvmTypeSymInfo {
        llvm::Type *LlvmType = nullptr;
        llvm::Module *LlvmMod = nullptr;

        auto Clone() const -> Unique<LlvmTypeSymInfo> {
            auto info = MakeUnique<LlvmTypeSymInfo>();
            info->LlvmType = LlvmType;
            info->LlvmMod = LlvmMod;
            return info;
        }
    };

    SPP_EXP_CLS struct LlvmVarSymInfo {
        llvm::Value *Alloca = nullptr;
    };
}
