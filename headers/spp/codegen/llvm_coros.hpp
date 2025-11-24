#pragma once

#include <llvm/IR/Value.h>


namespace spp::codegen {
    struct LlvmCoroFrame;
}


struct spp::codegen::LlvmCoroFrame {
    llvm::Value *coro_id;
    llvm::Value *coro_hdl;
};
