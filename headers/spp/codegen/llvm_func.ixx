module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_func;
import llvm;

namespace spp::codegen {
    SPP_EXP_CLS struct LlvmFuncWrapper;
}


/**
 * This is used as a shared pointer, wrapping the internal llvm::Function* pointer, allowing it to be shared between
 * cloned functions.
 */
SPP_EXP_CLS struct spp::codegen::LlvmFuncWrapper {
    llvm::Function *target;
};
