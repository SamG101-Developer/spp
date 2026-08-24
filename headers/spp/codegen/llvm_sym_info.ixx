module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_sym_info;
import spp.utils.types;
import llvm;
import std;

namespace spp::codegen {
  SPP_EXP_CLS struct LlvmTypeSymInfo {
    llvm::Type *LlvmType = nullptr;
    llvm::Module *LlvmMod = nullptr;
    Map<std::size_t, std::size_t> FieldIndexMap;
  };

  SPP_EXP_CLS struct LlvmVarSymInfo {
    /**
     * The LLVM allocation handle for the local variable being represented by a variable symbol.
     */
    llvm::Value *Alloca = nullptr;

    /**
     * For a local that is potentially moved from (in an @c case branch), we need to decide at runtime if we are to
     * destroy the stack allocation or not. This flag controls that.
     */
    llvm::Value *DropFlag = nullptr;
  };
}
