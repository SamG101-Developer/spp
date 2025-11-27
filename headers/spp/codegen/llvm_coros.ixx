module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_coros;
import llvm;


namespace spp::codegen {
    SPP_EXP_CLS struct LlvmCoroFrame;
}


SPP_EXP_CLS struct spp::codegen::LlvmCoroFrame {
    llvm::Value *coro_id;
    llvm::Value *coro_hdl;
};
