module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_coros;
import llvm;


namespace spp::codegen {
    SPP_EXP struct LlvmCoroFrame;
}


SPP_EXP struct spp::codegen::LlvmCoroFrame {
    llvm::Value *coro_id;
    llvm::Value *coro_hdl;
};
