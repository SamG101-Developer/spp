module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_sym_info;
import spp.utils.types;
import llvm;
import std;

namespace spp::codegen {
  SPP_EXP_CLS struct LlvmTypeSymbolInfo {
    /// The LLVM type for this symbol. Whilst bound here, it should not be
    /// accessed from here; prefer "GetLlvmType(...)".
    llvm::Type *LlvmType = nullptr;

    /// The field index map for the optimal ordering of fields to reduce object
    /// size and therefore memory usage.
    Map<std::size_t, std::size_t> FieldIndexMap;
  };

  SPP_EXP_CLS struct LlvmVarSymbolInfo {
    /// The llvm allocation for the local variable a variable symbol represents.
    llvm::Value *Alloca = nullptr;

    /// The type a global constant is read as, when its initialiser is laid out
    /// as that type in another shape (a variant member built as a packed
    /// struct); null to read the global's own.
    llvm::Type *ValueType = nullptr;

    /// The variable a debugger is told of, for a binding that can be moved (its
    /// type is not "Copy"): described by "#dbg_value"s of "Alloca", which a
    /// move ends and an assignment resumes. Null for any other binding.
    llvm::DILocalVariable *DiVariable = nullptr;
  };
}
