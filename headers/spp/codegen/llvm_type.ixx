module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_type;
import spp.asts;
import spp.analyse.scopes.symbols;
import spp.codegen.llvm_ctx;
import llvm;


namespace spp::codegen {
    SPP_EXP_FUN auto register_llvm_type_info(
        asts::ClassPrototypeAst const *cls_proto,
        LlvmCtx const *ctx)
        -> void;

    SPP_EXP_FUN auto llvm_type(
        analyse::scopes::TypeSymbol const &type_sym,
        LlvmCtx const *ctx)
        -> llvm::Type*;
}
