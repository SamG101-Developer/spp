module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_sym_info;
import spp.utils.types;
import llvm;
import std;

namespace spp::codegen {
  SPP_EXP_CLS struct LlvmTypeSymInfo {
    llvm::Type *LlvmType = nullptr;
    Map<std::size_t, std::size_t> FieldIndexMap;
  };

  SPP_EXP_CLS struct LlvmVarSymInfo {
    /**
     * The LLVM allocation handle for the local variable being represented by a variable symbol.
     */
    llvm::Value *Alloca = nullptr;

  };
}
