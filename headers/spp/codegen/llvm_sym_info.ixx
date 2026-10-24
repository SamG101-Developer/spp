module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_sym_info;
import spp.utils.types;
import llvm;
import std;

namespace spp::codegen {
  SPP_EXP_CLS struct LlvmTypeSymbolInfo {
    llvm::Type *LlvmType = nullptr;
    Map<std::size_t, std::size_t> FieldIndexMap;
  };

  SPP_EXP_CLS struct LlvmVarSymbolInfo {
    /// The llvm allocation for the local variable a variable
    /// symbol represents.
    llvm::Value *Alloca = nullptr;

    /// The type a global constant is read as, when its initializer
    /// is laid out as that type in another shape (a variant member
    /// built as a packed struct); null to read the global's own.
    llvm::Type *ValueType = nullptr;
  };
}
